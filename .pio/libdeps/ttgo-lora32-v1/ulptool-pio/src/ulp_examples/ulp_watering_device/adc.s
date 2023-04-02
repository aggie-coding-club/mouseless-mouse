/* ULP Example: using ADC in deep sleep

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   This file contains assembly code which runs on the ULP.

   ULP wakes up to run this code at a certain period, determined by the values
   in SENS_ULP_CP_SLEEP_CYCx_REG registers. On each wake up, the program
   measures input voltage on the given ADC channel 'adc_oversampling_factor'
   times. Measurements are accumulated and average value is calculated.
   Average value is compared to the two thresholds: 'low_thr' and 'high_thr'.
   If the value is less than 'low_thr' or more than 'high_thr', ULP wakes up
   the chip from deep sleep.
*/

/* ULP assembly files are passed through C preprocessor first, so include directives
   and C macros may be used in these files 
 */
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

  /* ADC1 channel 6, GPIO34 */
  .set adc_channel, 6

  /* Configure the number of ADC samples to average on each measurement.
     For convenience, make it a power of 2. */
  .set adc_oversampling_factor_log, 2
  .set adc_oversampling_factor, (1 << adc_oversampling_factor_log)

  /* Define variables, which go into .bss section (zero-initialized data) */
  .bss

  /* Low threshold of ADC reading.
     Set by the main program. */
  .global low_thr
low_thr:
  .long 0

  /* error threshold of ADC reading.
     Set by the main program. */
  .global error_thr
error_thr:
  .long 0

  /* Counter of measurements done */
  .global sample_counter
sample_counter:
  .long 0

  .global last_result
last_result:
  .long 0

  /* Code goes into .text section */
  .text
  .global entry
entry:
  
  /* increment sample counter */
  move r3, sample_counter
  ld r2, r3, 0
  add r2, r2, 1
  st r2, r3, 0

  /* do measurements using ADC */
  /* r0 will be used as accumulator */
  move r0, 0
  /* initialize the loop counter */
  stage_rst
measure:
  /* measure and add value to accumulator */
  adc r1, 0, adc_channel + 1
  add r0, r0, r1
  /* increment loop counter and check exit condition */
  stage_inc 1
  jumps measure, adc_oversampling_factor, lt

  /* divide accumulator by adc_oversampling_factor.
     Since it is chosen as a power of two, use right shift */
  rsh r0, r0, adc_oversampling_factor_log
  /* averaged value is now in r0; store it into last_result */
  move r3, last_result
  st r0, r3, 0

  /* compare with error_thr; wake up if value > error_thr */
  move r3, error_thr
  ld r3, r3, 0
  sub r3, r3, r0
  jump sensor_err, ov

  /* compare with low_thr; wake up if value < low_thr */
  move r3, low_thr
  ld r3, r3, 0
  sub r3, r0, r3
  jump stop_water, ov  /* if value < low_thr jump stop_water */

  .global start_water
start_water:
  /* Disable hold of RTC_GPIO10 output */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,0)

  /* Set the RTC_GPIO10 output HIGH
     to signal that ULP is now up */
  WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG,RTC_GPIO_OUT_DATA_W1TS_S+10,1,1)
  
  /* Enable hold on RTC_GPIO10 output */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,1)

  /* stop water and go sleep */ 
  jump exit /* start pump and go sleep wait next wake up checking ADC value */

  .global stop_water
stop_water:
  /* Disable hold of RTC_GPIO10 output */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,0)

  /* Set the RTC_GPIO10 output LOW (clear output)
     to signal that ULP is now going down */
  WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG,RTC_GPIO_OUT_DATA_W1TC_S+10,1,1)

  /* Enable hold on RTC_GPIO10 output */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,1)

  jump exit /* stop water and go sleep */

  .global sensor_err
sensor_err:
  /* Disable hold of RTC_GPIO10 output */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,0)

  /* Set the RTC_GPIO10 output LOW (clear output)
     to signal that ULP is now going down */
  WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG,RTC_GPIO_OUT_DATA_W1TC_S+10,1,1)

  /* Enable hold on RTC_GPIO10 output */
  WRITE_RTC_REG(RTC_IO_TOUCH_PAD0_REG,RTC_IO_TOUCH_PAD0_HOLD_S,1,1) 

  jump wake_up  /* sensor error waku up */  

  /* value within range, end the program */
  .global exit
exit:
  halt

  .global wake_up
wake_up:
  /* Check if the system can be woken up */
  READ_RTC_REG(RTC_CNTL_DIAG0_REG, 19, 1)
  and r0, r0, 1
  jump exit, eq

  /* Wake up the SoC, end program */
  wake
  WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
  halt
