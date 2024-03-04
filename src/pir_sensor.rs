use core::future;

use portable_atomic::AtomicU64;

use embassy_rp::{bind_interrupts, gpio::Input, peripherals::PIN_23};
use embedded_hal::digital::InputPin;



static PIR_SENSOR_LAST_DETECTION: AtomicU64 = AtomicU64::new(0);

#[embassy_executor::task]
pub async fn pir_task(mut pir_sensor_input: Input<'static, PIN_23>) {
    loop {
        pir_sensor_input.wait_for_high().await;

        PIR_SENSOR_LAST_DETECTION.store(embassy_time::Instant::now().as_micros(), core::sync::atomic::Ordering::Relaxed);

        futures::join!(
            Timer::after(core::time::Duration::from_secs(5)),
            pir_sensor_input.wait_for_low()
        ).await;

    }
    
}
