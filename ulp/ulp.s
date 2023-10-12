#include "soc/soc_ulp.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"

/* Define variables, which go into .bss section (zero-initialized data) */
    .bss
/* Store count value */
    .global ctr             // Global counter variable - is accessible to the main program by the identifier `ulp_ctr`
ctr:
    .long 0

/* Code goes into .text section */
    .text
    .global entry           // Pointer to the execution entry point for the program (the main system will use this to start the ULP coprocessor)
entry:
    move    r3, ctr         // Load the address of the variable `ctr` into register 3
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)   // Read the RTC GPIO pin states into register 0
    and     r0, r0, 2048    // Mask register 0 with hex 0x0800, store the result in register 0 - this value is nonzero if pin GPIO 0 is high
    jumpr   press, 1, LT    // If pin GPIO 0 is low, the button is pressed, so skip past the `halt` instruction
    ld      r0, r3, 0       // Load the counter variable's value from memory into register 0
    jumpr   nopress, 1, LT  // The sleep cycle time register will usually be set correctly, so skip this next bit if it hasn't been modified
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 0, 8, 0xF0)   // Write the number 150,000 to the ULP sleep cycle time register
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 8, 8, 0x49)   // 8 bits at a time
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 16, 8, 0x02)  // I'm bored :(
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 24, 8, 0)     // This timer runs at 150kHz, so the value 150,000 will run the ULP program once per second
nopress:                    // Just a label - execution can jump here from `jumpr`
    move    r0, 0           // Put the number 0 in register 0
    st      r0, r3, 0       // Reset the counter variable by storing the 0 to its address
    halt                    // Will only be executed if the button is not pressed
press:                      // Just a label - execution can jump here from `jumpr`
    ld      r0, r3, 0       // Load the counter variable's value from memory into register 0
    jumpr   fast, 1, GE     // Skip the cycle time adjustment if we've already made the change
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 0, 8, 0x98)   // Write the number 15,000 to the ULP sleep cycle time register
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 8, 8, 0x3A)   // 8 bits at a time
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 16, 8, 0)     // Making sure to clear out the more significant bytes
    WRITE_RTC_REG(SENS_ULP_CP_SLEEP_CYC0_REG, 24, 8, 0)     // This timer runs at 150kHz, so the value 15,000 will run the ULP program 10 times a second
fast:
    add     r0, r0, 1       // Increment the local copy of the counter variable's value
    jumpr   btnheld, 20, GE // Check whether the counter variable has reached 20 - this means the button has been pressed for 20 consecutive iterations
    st      r0, r3, 0       // Store the updated counter value to the counter variable's location in memory
    halt                    // Halt the ULP until it's time for the next iteration
btnheld:                    // Execution will jump here when the button has been held long enough to initiate the wakeup sequence
    ; WRITE_RTC_REG(RTC_GPIO_ENABLE_W1TS_REG, RTC_GPIO_ENABLE_W1TS_S + 16, 1, 1) // Enable output on pin GPIO 14 THIS WAS CHANGED ASSEMBLY CODE
    ; WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S + 16, 1, 1)  // Set pin GPIO 14 high THIS WAS CHANGED ASSEMBLY CODE
    ; wait    8000            // 8MHz / 8000 = 1 ms delay for analog signals to propagate THIS WAS CHANGED ASSEMBLY CODE
    adc     r0, 0, 7        // Read ADC 1 (zero-based) channel 6 (one-based) into register 0 (what were the devs smoking?)
    st      r0, r3, 0       // We don't need the counter variable anymore, so store the ADC reading into it just for funzies
    jumpr   lowbat, 1805, LT    // If the battery is low, don't wake up
trywake:
    READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)  // Check that the RTC controller is ready to wake up the system
    wake                    // Ask for forgiveness, not permission
    jumpr   trywake, 1, LT  // If the RTC controller wasn't ready, try again (this shouldn't really happen, but if it does, here we go)
lowbat:                     // Let execution fall through after waking the system - the ULP has done its job, so we're ready to stop it
    WRITE_RTC_REG(RTC_IO_DIG_PAD_HOLD_REG, RTC_IO_DIG_PAD_HOLD_S + 11, 1, 0)  // Disable the internal pullup on GPIO 0 to conserve power
    WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)     // Stop the ULP wakeup timer - the ULP will not wake up again
    halt                    // Goodbye ULP, we hardly knew ye