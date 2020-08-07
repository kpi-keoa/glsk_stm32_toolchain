/**
 * This example demonstrates use of semihosting feature.
 *
 * Semihosting allows to output information such as generic stdout or debug log
 * from MCU to PC via debugger.
 * The end result is: we use regular printf() and see its output in terminal
 * as if the program was running on the PC.
 *
 * Semihosting puts data in MCU buffer, then traps MCU and waits for debugger
 * to respond. Debugger communicates with OpenOCD, which reads data from buffer
 * in a way similar to reading variable values during gdb session. And then
 * returns control to MCU.
 * Semihosting does not require any specific hardware features. But it is slow.
 * Usually slower than SWO ITM as SWO could output data directly on single pin
 * at the same time when MCU is busy doing some real work, without need to stop
 * and trap MCU.
 *
 * Semihosting is very useful for debugging. But the downside is that the firmware
 * will not work standalone without the debugger being connected.
 * We can overcome this by introducing two build profiles: debug and release
 *
 * HOW TO USE SEMIHOSTING:
 * 1. Link with `--specs=rdimon.specs -lrdimon` and DO NOT use `-lnosys`
 * 2. In OpenOCD use command `arm semihosting enable` before initialise_monitor_handles
 *    is first called by your program. If you are using openocd directly, that's enough.
 *
 *    If you are using OpenOCD run via GDB pipe, in GDB session run:
 *        monitor arm semihosting enable
 *        monitor arm semihosting_fileio enable
 *    and you'll probably need to reset MCU, in GDB session:
 *        monitor reset halt
 * 3. Don't forget to use `initialise_monitor_handles` in your program code
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>

// USE_SEMIHOSTING is defined via our Makefile
#if defined(USE_SEMIHOSTING) && USE_SEMIHOSTING
// Usually we try not to use standard C library. But in this case we need printf
#include <stdio.h>

// this is our magic from librdimon
// should be called in main before first output
extern void initialise_monitor_handles(void);
#else
// this is how semihosting may be used conditionally
#error "This example requires SEMIHOSTING=1"
#endif


void softdelay(volatile uint32_t N)
{
    while (N--);
}


int main(void)
{
	initialise_monitor_handles();
	printf("Monitor initialized. WE GET IT THROUGH SEMIHOSTING\n");
    rcc_periph_clock_enable(RCC_GPIOD);
    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12);
    gpio_set(GPIOD, GPIO12);
	printf("System initialized\n");
    while (1) {
		gpio_toggle(GPIOD, GPIO12);
        softdelay(800000);
	}
}
