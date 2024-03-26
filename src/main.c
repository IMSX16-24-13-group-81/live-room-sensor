#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <stdio.h>

int main() {
  stdio_init_all();

  if (cyw43_arch_init_with_country(CYW43_COUNTRY_SWEDEN)) {
    printf("CYW43 init failed\n");
    return -1;
  }

  timer_hw->dbgpause = 0;
}
