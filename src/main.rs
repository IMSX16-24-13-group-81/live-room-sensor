#![no_std]
#![no_main]

use cyw43_pio::PioSpi;
use cyw43::SpiBusCyw43;
use defmt::*;
use embassy_executor::Spawner;
use embassy_net::{tcp::client::{TcpClient, TcpClientState}, Stack};
use embassy_rp::{
    bind_interrupts, gpio::{Input, Level, Output}, peripherals::PIO0, pio::{InterruptHandler, Pio}
};
use embassy_time::Timer;
use embedded_hal::digital::OutputPin;
use reqwless::{client::{HttpClient, TlsConfig, TlsVerify}, headers, request::{RequestBody, RequestBuilder}};
use serde::Serialize;

use {defmt_rtt as _, panic_probe as _};

mod wifi;
mod config;
mod pir_sensor;

use crate::config::CONFIG;

const FIRMWARE_VERSION: &str = env!("CARGO_PKG_VERSION");

static mut MAC_ADDRESS : [u8; 12] = [0; 12];


bind_interrupts!(struct Irqs {
    PIO0_IRQ_0 => InterruptHandler<PIO0>;
});

#[embassy_executor::task]
async fn wifi_task(
    runner: cyw43::Runner<'static, impl OutputPin + 'static, impl SpiBusCyw43 + 'static>,
) -> ! {
    runner.run().await
}

#[embassy_executor::task]
pub async fn net_task(stack: &'static Stack<cyw43::NetDriver<'static>>) -> ! {
    stack.run().await
}

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    crate::config::validate_config().unwrap();
    let p = embassy_rp::init(Default::default());

    let pwr = Output::new(p.PIN_23, Level::Low);
    let cs = Output::new(p.PIN_25, Level::High);
    let mut pio = Pio::new(p.PIO0, Irqs);
    let spi = PioSpi::new(
        &mut pio.common,
        pio.sm0,
        pio.irq0,
        cs,
        p.PIN_24,
        p.PIN_29,
        p.DMA_CH0,
    );

    spawner.spawn(pir_sensor::pir_task(p.PIN_28)).unwrap();

    let (mut control, stack) = wifi::init_wifi(pwr, spi, &spawner).await;
    let stack = &*wifi::STACK.init(stack);
    wifi::start_network_stack(stack, &spawner);

    loop {
        //control.join_open(WIFI_NETWORK).await;
        match control.join_wpa2(CONFIG.wifi_ssid, CONFIG.wifi_password).await {
            Ok(_) => break,
            Err(err) => {
                info!("join failed with status={}", err.status);
            }
        }
    }

    // Wait for DHCP, not necessary when using static IP
    info!("waiting for DHCP...");
    while !stack.is_config_up() {
        Timer::after_millis(100).await;
    }
    info!("DHCP is now up!");

    // And now we can use it!

    let mut tls_read_buf: [u8; 16640] = [0; 16640];
    let mut tls_write_buf: [u8; 8096] = [0; 8096];
    let dns_client = embassy_net::dns::DnsSocket::new(stack);
    let tcp_client_state: TcpClientState<1, 4096, 4096> = TcpClientState::new();
    let tcp_client = TcpClient::new(stack, &tcp_client_state);
    let tls_config = TlsConfig::new(0xdeadbeef,&mut tls_read_buf, &mut tls_write_buf, TlsVerify::None);
    let mut http_client = HttpClient::new_with_tls(&tcp_client, &dns_client, tls_config);

    let mut rx = [0; 4096];
    let mut tx = [0; 4096];

    let hardware_address = stack.hardware_address();
    let hardware_address = hardware_address.as_bytes();
    let mac_string;
    unsafe{
        hex::encode_to_slice(&hardware_address, &mut MAC_ADDRESS);
        mac_string = core::str::from_utf8(&MAC_ADDRESS).unwrap();
    }
    

    let mut sensor_report = SensorReport{
        sensor_id: mac_string,
        firmware_version: FIRMWARE_VERSION,
        auth_token: CONFIG.auth_token,
        occupants: 0,

    };

    let headers = [
        ("Authorization", CONFIG.auth_token)
    ];

    loop {
    
        Timer::after_secs(60).await;
        info!("sending report");
        sensor_report.occupants = if pir_sensor::get_last_detection() + CONFIG.pir_delay*1000000 > embassy_time::Instant::now().as_micros(){
            1
        } else {
            0
        };
        let serialization_result = serde_json_core::to_slice(&sensor_report, &mut tx);
        if serialization_result.is_err() {
            error!("serialization error");
            continue;
        }

        info!("sending report: {:?}", core::str::from_utf8(&tx[..serialization_result.unwrap()]).unwrap_or("couldn't parse"));
        let mut request = 
        http_client.request(reqwless::request::Method::POST, "https://liveinfo.spacenet.se/api/sensors/report").await
        .unwrap()
        .body(&tx[..serialization_result.unwrap()])
        .content_type(headers::ContentType::ApplicationJson)
        .headers(&headers);

        let response = request.send(&mut rx).await.unwrap();
        info!("response received, status: {:?}", response.status);

        let body = response.body().read_to_end().await.unwrap();
        
        let res = core::str::from_utf8(body).unwrap_or("couldn't parse");
        info!("response: {:?}", res);

    }
}


#[derive(Serialize)]
struct SensorReport {
    #[serde(rename = "sensorId")]
    sensor_id: &'static str,
    #[serde(rename = "firmwareVersion")]
    firmware_version: &'static str,
    #[serde(rename = "authorization")]
    auth_token: &'static str,
    #[serde(rename = "occupants")]
    occupants: u8,

}