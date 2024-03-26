#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "wifi.h"
#include <stdio.h>

int main() {
  timer_hw->dbgpause = 0;
  stdio_init_all();

  if (watchdog_caused_reboot()) {
    printf("Rebooted by Watchdog!\n");
  } else {
    printf("Clean boot\n");
  }

  // Initialise Pico W wireless hardware
  printf("Initializing CYW43\n");
  if (cyw43_arch_init_with_country(CYW43_COUNTRY_SWEDEN)) {
    printf("CYW43 init failed\n");
    goto RESET;
  }
  printf("Initialized CYW43\n");

  // Connect to wireless network
  printf("Connecting to %s\n", WIFI_SSID);
  int wifi_connect_result = cyw43_arch_wifi_connect_timeout_ms(
      WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
  if (!wifi_connect_result) {
    printf("Failed to connect to %s with error code %d\n", WIFI_SSID,
           wifi_connect_result);
    goto RESET;
  }
  printf("Connected to %s\n", WIFI_SSID);

  // Enable the watchdog, requiring the watchdog to be updated every 1000ms or
  // the chip will reboot second arg is pause on debug which means the watchdog
  // will pause when stepping through code
  watchdog_enable(1000, 1);

RESET:
  // Deinit Pico W wireless hardware
  cyw43_arch_deinit();
  printf("Exiting\n");
  watchdog_reboot(0, 0, 0);
  return 0;
}
