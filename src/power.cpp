#include "power.h"
#include "display.h"
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp32/ulp.h>
// ULP header in ./ulp - links ULP entry points and variables to main program
#include "ulp_main.h"
// Need to include custom binary loader
#include "ulptool.h"
// Unlike the esp-idf always use these binary blob names
extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

extern Display display;

// Put the whole T-Display board into a deep sleep state
void deepSleep() {
  display.flush();                // Clear the display
  vTaskDelay(pdMS_TO_TICKS(100)); // Give the display driver a little time
  display.sleepMode();            // Put the display driver to sleep
  ulp_ctr = 0;                    // Initialize global ulp variable
  digitalWrite(14, HIGH);
  rtc_gpio_hold_en(GPIO_NUM_14);
  rtc_gpio_hold_en(GPIO_NUM_0);   // Keep the pullup resistor on pin 0 powered during deep sleep
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_ulp_enable();                 // Configure ADC 1 for use with the ULP coprocessor
  ulp_set_wakeup_period(0, 1000000); // Run the ULP coprocessor's routine once per second
  esp_err_t err =
      ulptool_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
  err = ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t));
  esp_sleep_enable_ulp_wakeup(); // Allow the ULP to wake the system
  esp_deep_sleep_start();        // Nighty-night
}

// Stop the ULP coprocessor from running
void stopULP() {
  ulp_timer_stop(); // Stop the timer that wakes up the ULP coprocessor
}