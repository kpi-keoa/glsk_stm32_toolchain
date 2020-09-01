#include "sync.h"
#include "pin.h"
#include "tick.h"
#include "macro.h"
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

int main(void)
{
	rcc_periph_clock_enable(RCC_GPIOD);		// DISCOVERY LEDS are connected to Port E
	glsk_pins_init(false);

	sk_pin_set(sk_io_led_blue, false);
	sk_pin_set(sk_io_led_green, true);	// turn on LED until everything is configured
	clock_init();
	sk_pin_set(sk_io_led_green, false);

	sk_tick_init(rcc_ahb_frequency / 10000ul, (2 << 2 | 0));

	cm_enable_interrupts();

	sk_pin_set(sk_io_led_orange, true);
	sk_tick_delay_ms(500);


	sk_lock_t tstlock = __SK_LOCK_UNLOCKED;

	// code between successfully captured lock, and release of that lock will be protected
	// from other routines using the same lock

	bool success = sk_lock_trylock(&tstlock);		// no one else captured, so we can capture it

	sk_pin_set(sk_io_led_blue, success);
	// sk_pin_set(sk_io_led_blue, sk_lock_trylock(&tstlock)); // this fails, but returns instantly
	// sk_lock_spinlock(&tstlock);	// will cause deadlock as lock is already captured and has not been released
	sk_pin_set(sk_io_led_orange, false);

	sk_lock_unlock(&tstlock);


	while (1) {
		sk_tick_delay_ms(500);
		sk_pin_toggle(sk_io_led_red);
	}
}
