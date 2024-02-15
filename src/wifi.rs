use cyw43::SpiBusCyw43;
use defmt::unwrap;
use embassy_executor::Spawner;
use embassy_net::{Config, Stack, StackResources};
use embedded_hal::digital::OutputPin;
use static_cell::StaticCell;

const RANDOM_SEED: u64 = 0xdeadbeef;

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

pub static STACK: StaticCell<Stack<cyw43::NetDriver<'static>>> = StaticCell::new();
static RESOURCES: StaticCell<StackResources<2>> = StaticCell::new();

pub async fn init_wifi(
        pwr: impl OutputPin + 'static,
        spi: impl SpiBusCyw43 + 'static,
        spawner: &Spawner,
    ) -> (cyw43::Control<'static>, Stack<cyw43::NetDriver<'static>>) {
        let fw = include_bytes!("../cyw43-fw/43439A0.bin");
        let clm = include_bytes!("../cyw43-fw/43439A0_clm.bin");

        static STATE: StaticCell<cyw43::State> = StaticCell::new();
        let state = STATE.init(cyw43::State::new());

        let (net_device, mut control, runner) = cyw43::new(state, pwr, spi, fw).await;
        unwrap!(spawner.spawn(wifi_task(runner)));

        control.init(clm).await;
        control
            .set_power_management(cyw43::PowerManagementMode::PowerSave)
            .await;

        let config = Config::dhcpv4(Default::default());

        let stack = Stack::new(
            net_device,
            config,
            RESOURCES.init(StackResources::<2>::new()),
            RANDOM_SEED,
        );

        (control, stack)
    }


pub fn start_network_stack(wifi: &'static Stack<cyw43::NetDriver<'static>>, spawner: &Spawner) {
    unwrap!(spawner.spawn(net_task(wifi)));
}
