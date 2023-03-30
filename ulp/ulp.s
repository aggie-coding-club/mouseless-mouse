#include "soc/soc_ulp.h"
#include "soc/rtc_io_reg.h"
/* Define variables, which go into .bss section (zero-initialized data) */
    .bss
/* Store count value */
    .global ctr
ctr:
    .long 0

/* Code goes into .text section */
    .text
    .global entry
entry:
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)   // Read the RTC GPIO pin states into register 0
    and     r0, r0, 2048    // Mask register 0 with hex 0x0800, store the result in register 0 - this value is nonzero if pin 0 is high
    jumpr   nowake, 1, GE   // If pin 0 is high, it's being pulled up - the button is not pressed, so skip the `wake` instruction
    wake                    // Will only be executed if the button is pressed
nowake:                     // Just a label - execution can either jump here from `jumpr` or fall through after `wake`
    halt                    // Regardless of the branch we took, halt the ULP coprocessor at the end of the routine