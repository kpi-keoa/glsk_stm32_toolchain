#include "FreeRTOS.h"
#include "task.h"
#include "tick.h"

#include "pin.h"
#include "macro.h"
#include <libprintf/printf.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/rcc.h>
#include <stdint.h>
#include <stddef.h>


static void clock_init(void)
{
	// Set clock to 168 MHz (external 8 MHz generator input from on-board ST-Link
	// multiplied with PLL
	// For more detailed description, see example "168mhz_extclk.c" in 06_clock_system

	rcc_osc_bypass_enable(RCC_HSE);		// bypass load capacitors used for crystals
	rcc_osc_on(RCC_HSE);				// enable High-Speed External clock (HSE)
	while (!rcc_is_osc_ready(RCC_HSE));	// trap until external clock is detected as stable

	rcc_osc_off(RCC_PLL);		// Disable PLL before configuring it

	// * Set PLL multiplication factor as specified at page 226 RM
	// PLLM = 4		-- 8/4 = 2 MHz input to PLL mul stage
	// PLLN = 168   -- F<main> = 2 * 168 = 336 MHz
	// PLLP = 2		-- F<genout> = 336 / 2 = 168 MHz for our CPU and AHB
	// PLLQ = 7		-- F<Qdomain> = 336 / 7 = 48 MHz exactly
	rcc_set_main_pll_hse(4, 168, 2, 7, 0);		// with input from HSE to PLL
	rcc_css_disable();		// Disable clock security system
	rcc_osc_on(RCC_PLL);				// Enable PLL
	while (!rcc_is_osc_ready(RCC_PLL)); // Wait for PLL out clock to stabilize

	// Set all prescalers.
	rcc_set_hpre(RCC_CFGR_HPRE_DIV_NONE);	// AHB = 168 / 1 = 168 MHz
    rcc_set_ppre1(RCC_CFGR_PPRE_DIV_4);		// APB1 = FAHB / 4 = 168 / 4 = 42 MHz  (<= 42 MHz)
    rcc_set_ppre2(RCC_CFGR_PPRE_DIV_2);		// APB2 = FAHB / 2 = 168 / 2 = 84 MHz  (<= 84 MHz)

	// Enable caches. Flash is slow (around 30 MHz) and CPU is fast (168 MHz)
	flash_dcache_enable();
	flash_icache_enable();

	flash_set_ws(FLASH_ACR_LATENCY_7WS); // IMPORTANT! We must increase flash wait states (latency)


    rcc_set_sysclk_source(RCC_CFGR_SW_PLL);		// Select PLL as AHB bus (and CPU clock) source
	rcc_wait_for_sysclk_status(RCC_PLL);		// Wait for clock domain to be changed to PLL input

	// set by hand since we've not used rcc_clock_setup_pll
	rcc_ahb_frequency = 168000000ul;
	rcc_apb1_frequency = rcc_ahb_frequency / 4;
	rcc_apb2_frequency = rcc_ahb_frequency / 2;
	rcc_osc_off(RCC_HSI);		// Disable internal 16 MHz RC oscillator (HSI)
}


static inline TickType_t ticks_from_ms(uint32_t ms)
{
	return (configTICK_RATE_HZ * ms) / 1000;
}



/*
static void task_blink_led_blue(void *args)
{
	uint32_t *ptr = args;		// convert back since we know the type (we passed it)
	uint32_t period_ms = *ptr;	// get value. This will be local variable allocated on stack
	vPortFree(ptr);		// now we don't need the memory, so free it

	while (1) {
		sk_pin_toggle(sk_io_led_blue);
		vTaskDelay(ticks_from_ms(period_ms));
	}

	// we should not get here
}
*/


struct task_blink_args {
	uint32_t period_ms;
	sk_pin pin;
};


static void task_blink_led(void *args)
{
	struct task_blink_args *targ = args;		// convert back since we know the type (we passed it)
	uint32_t period_ms = targ->period_ms;	// get value. This will be local variable allocated on stack
	sk_pin pin = targ->pin;
	vPortFree(targ);		// now we don't need the memory, so free it

	while (1) {
		sk_pin_toggle(pin);
		vTaskDelay(ticks_from_ms(period_ms));
	}

	// we should not get here
}


int main(void)
{
	rcc_periph_clock_enable(RCC_GPIOD);		// DISCOVERY LEDS are connected to Port E
	glsk_pins_init(false);

	sk_pin_set(sk_io_led_green, true);	// turn on LED until everything is configured
	clock_init();
	sk_pin_set(sk_io_led_green, false);

	sk_pin_set(sk_io_led_orange, true);

	// now we have dynamic memory allocation.. nice
	// uint32_t *blue_period_ms = pvPortMalloc(sizeof(*blue_period_ms));
	// *blue_period_ms = 500;

	struct task_blink_args *blue = pvPortMalloc(sizeof(*blue)),
						   *orange = pvPortMalloc(sizeof(*blue));
	*blue = (struct task_blink_args) { .period_ms = 700, .pin = sk_io_led_blue };
	*orange = (struct task_blink_args) { .period_ms = 300, .pin = sk_io_led_orange };

	// TaskHandle_t blue_blink_task_hdl;
	// xTaskCreate(&task_blink_led_blue,
    //            "blueblink",
    //            5 * configMINIMAL_STACK_SIZE,
    //            blue_period_ms,
    //            3,
    //            NULL);

	xTaskCreate(&task_blink_led, "blue", 5 * configMINIMAL_STACK_SIZE, blue, 3, NULL);
	xTaskCreate(&task_blink_led, "orange", 5 * configMINIMAL_STACK_SIZE, orange, 3, NULL);


	// Start the real time scheduler.
    vTaskStartScheduler();
	
	// we should not reach this code
	while (1) {
		
	}
}
