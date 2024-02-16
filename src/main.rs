#![no_std]
#![no_main]

use cyw43_pio::PioSpi;
use cyw43::SpiBusCyw43;
use defmt::*;
use embassy_executor::Spawner;
use embassy_net::{tcp::client::{TcpClient, TcpClientState}, Config, Stack, StackResources};
use embassy_rp::{
    bind_interrupts, gpio::{Level, Output}, peripherals::PIO0, pio::{InterruptHandler, Pio}
};
use embassy_time::Timer;
use embedded_hal::digital::OutputPin;
use reqwless::client::HttpClient;
use static_cell::StaticCell;

use {defmt_rtt as _, panic_probe as _};

mod wifi;

const WIFI_NETWORK: &str = "Oscar-WLAN";
const WIFI_PASSWORD: &str = "Oscariremma";

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
    let p = embassy_rp::init(Default::default());

    // To make flashing faster for development, you may want to flash the firmwares independently
    // at hardcoded addresses, instead of baking them into the program with `include_bytes!`:
    //     probe-rs download 43439A0.bin --format bin --chip RP2040 --base-address 0x10100000
    //     probe-rs download 43439A0_clm.bin --format bin --chip RP2040 --base-address 0x10140000
    //let fw = unsafe { core::slice::from_raw_parts(0x10100000 as *const u8, 230321) };
    //let clm = unsafe { core::slice::from_raw_parts(0x10140000 as *const u8, 4752) };

    //let config = embassy_net::Config::ipv4_static(embassy_net::StaticConfigV4 {
    //    address: Ipv4Cidr::new(Ipv4Address::new(192, 168, 69, 2), 24),
    //    dns_servers: Vec::new(),
    //    gateway: Some(Ipv4Address::new(192, 168, 69, 1)),
    //});

    // Init network stack

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

    let (mut control, stack) = wifi::init_wifi(pwr, spi, &spawner).await;
    let stack = &*wifi::STACK.init(stack);
    wifi::start_network_stack(stack, &spawner);

    loop {
        //control.join_open(WIFI_NETWORK).await;
        match control.join_wpa2(WIFI_NETWORK, WIFI_PASSWORD).await {
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

    let dns_client = embassy_net::dns::DnsSocket::new(stack);
    let tcp_client_state: TcpClientState<4, 4096, 4096> = TcpClientState::new();
    let mut tcp_client = TcpClient::new(stack, &tcp_client_state);
    let mut http_client = HttpClient::new(&mut tcp_client, &dns_client);

    let mut rx = [0; 4096];

    loop {

        control.gpio_set(0, false).await;
    
        Timer::after_secs(5).await;
        let mut request = 
        http_client.request(reqwless::request::Method::GET, "http://google.com").await
        .unwrap();
        let response = request.send(&mut rx).await;
    
        match response {
            Ok(response) => {
                let body = response.body().body_buf;
                info!("response: {}", body);
            }
            Err(err) => {
                info!("request failed: {:?}", err);
            }
            
        }
    
        
        control.gpio_set(0, true).await;
    

    }
}
