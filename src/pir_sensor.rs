use core::future;

use embassy_time::Timer;
use portable_atomic::AtomicU64;

use embassy_rp::{bind_interrupts, gpio::Input, peripherals::PIN_28};
use embedded_hal::digital::InputPin;



static PIR_SENSOR_LAST_DETECTION: AtomicU64 = AtomicU64::new(0);

pub fn get_last_detection() -> u64 {
    PIR_SENSOR_LAST_DETECTION.load(core::sync::atomic::Ordering::Relaxed)
}

#[embassy_executor::task]
pub async fn pir_task(pin : PIN_28) {

    let mut pir_sensor_input = Input::new(pin, embassy_rp::gpio::Pull::Down);

    loop {
        pir_sensor_input.wait_for_high().await;

        PIR_SENSOR_LAST_DETECTION.store(embassy_time::Instant::now().as_micros(), core::sync::atomic::Ordering::Relaxed);
        
        Timer::after(embassy_time::Duration::from_secs(5)).await;

    }
    
}
