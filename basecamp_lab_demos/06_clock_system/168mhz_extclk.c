#include "pin.h"
#include "tick.h"
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/pwr.h>
#include <stdint.h>
#include <stddef.h>


static void clock_init(void)
{
	// * Until now we have been working with internal 16 MHz RC oscillator using default config
	//   Let's switch to higher 168 MHz frequency and use more precise external crystal oscillator
	// * On Discovery board we have two crystal oscillators: X1 and X2.
	//   X1 is used to clock on-board ST-Link, which in default configuration outputs clock to MCU
	//   X2 can clock MCU directly, but this requires desoldering (removing) R68 resistor
	//      to free the line. Obviously we're not interested in this option.
	//   So, we will use ST-Link MCO (Master Clock Output) from X1 to provide 8 MHz external
	//   clock for our MCU.
	// * MCO is derived from X1 is 8 MHz, 30 ppm (+- 240 Hz) external crystal oscillator

	// * We will get this 8 MHz clock and multipy it on internal MCU PLL by factor of x21
	//   to get 168 MHz and use this resulting frequency to clock AHB bus

	// After Reset, we are running from internal 16 MHz clock

	// Set HCE to external clock source (HSE bypass), not crystal. See page 218 RM
	rcc_osc_bypass_enable(RCC_HSE);		// bypass load capacitors used for crystals
	rcc_osc_on(RCC_HSE);				// enable High-Speed External clock (HSE)
	// trap until external clock is detected as stable
	while (!rcc_is_osc_ready(RCC_HSE));


	// as page 79 DS tells to operate at high frequency, we need to supply more power
	// by setting VOS=1 bit in power register
	rcc_periph_clock_enable(RCC_PWR);
	pwr_set_vos_scale(PWR_SCALE1);
	rcc_periph_clock_disable(RCC_PWR);	// no need to configure or use features requiring clock

	// Configure PLL
	rcc_osc_off(RCC_PLL);		// Disable PLL before configuring it

	// * Set PLL multiplication factor as specified at page 226 RM
	//   F<main> = Fin * PLLN / PLLM		-- main PLL clock (intermediate)
	//   F<genout> = F<main> / PLLP			-- AHB clock output
	//   F<Qdomain> = F<main> / PLLQ		-- Q clock domain 
	//										(must be <= 48 MHz or exactly 48 MHz if HS-USB is used)
	// * rcc_set_main_pll_hse(PLLM, PLLN, PLLP, PLLQ, PLLR)
	// 		PLLM		Divider for the main PLL input clock
	// 		PLLN		Main PLL multiplication factor for VCO
	// 		PLLP		Main PLL divider for main system clock
	// 		PLLQ		Main PLL divider for USB OTG FS, SDMMC & RNG
	// 		0 PLLR		Main PLL divider for DSI (for parts without DSI, provide 0 here)
	// * PLL has limitations:
	//   PLLM:	2 <= PLLM <= 63. After PLLM division frequency must be from 1 MHz to 2 MHz
	//					         Whereas 2 MHz is recommended
	//	 	So, set PLLM = 8 MHz / 2 MHz = /4 division
	//   PLLN:  50 <= PLLN <= 432. After PLLN multiplication frequency must be from 100 to 432 MHz
	//   PLLP:  PLLP is /2, /4, /6 or /8. After PLLP division, frequency goes to AHB and sysclk
	//							  And should not exceed 168 MHz
	//   PLLQ:  2 <= PLLQ <= 15. After PLLQ division frequency must be below 48 MHz
	//						     and exactly 48 MHz if SDIO peripheral is used
	//	    So, select PLLQ to achieve 48 MHz
	// The resulting solution is:
	// PLLM = 4		-- 8/4 = 2 MHz input to PLL mul stage
	// PLLN = 168   -- F<main> = 2 * 168 = 336 MHz
	// PLLP = 2		-- F<genout> = 336 / 2 = 168 MHz for our CPU and AHB
	// PLLQ = 7		-- F<Qdomain> = 336 / 7 = 48 MHz exactly
	rcc_set_main_pll_hse(4, 168, 2, 7, 0);		// with input from HSE to PLL
	rcc_css_disable();		// Disable clock security system
	rcc_osc_on(RCC_PLL);				// Enable PLL
	while (!rcc_is_osc_ready(RCC_PLL)); // Wait for PLL out clock to stabilize


	// Set all prescalers.
	// (!) Important. Different domains have different maximum allowed clock frequencies
	//     So we need to set corresponding prescalers before switching AHB to PLL
	// AHB should not exceed 168 MHZ		-- divide 168 MHz by /1
	// APB1 should not exceed 42 MHz		-- divide AHB by /4 = 168 / 4 = 42 MHz
	// APB2 should not exceed 84 MHz		-- divide AHB by /2 = 168 / 2 = 84 MHz
	rcc_set_hpre(RCC_CFGR_HPRE_DIV_NONE);
    rcc_set_ppre1(RCC_CFGR_PPRE_DIV_4);
    rcc_set_ppre2(RCC_CFGR_PPRE_DIV_2);

	// Enable caches. Flash is slow (around 30 MHz) and CPU is fast (168 MHz)
	flash_dcache_enable();
	flash_icache_enable();

	// IMPORTANT! We must increase flash wait states (latency)
	// otherwise fetches from flash will ultimately fail
	flash_set_ws(FLASH_ACR_LATENCY_7WS);

	// Select PLL as AHB bus (and CPU clock) source
    rcc_set_sysclk_source(RCC_CFGR_SW_PLL);
	// Wait for clock domain to be changed to PLL input
	rcc_wait_for_sysclk_status(RCC_PLL);

	// set by hand since we've not used rcc_clock_setup_pll
	rcc_ahb_frequency = 168000000ul;
	rcc_apb1_frequency = rcc_ahb_frequency / 4;
	rcc_apb2_frequency = rcc_ahb_frequency / 2;

	// Disable internal 16 MHz RC oscillator (HSI)
	rcc_osc_off(RCC_HSI);
}


int main(void)
{
	rcc_periph_clock_enable(RCC_GPIOD);		// DISCOVERY LEDS are connected to Port E
	glsk_pins_init(false);

	sk_pin_set(sk_io_led_green, true);	// turn on LED until everything is configured
	clock_init();
	sk_pin_set(sk_io_led_green, false);

	sk_tick_init(168000000ul / 10000ul, (2 << 2 | 0));
	cm_enable_interrupts();

	while (1) {
		sk_pin_toggle(sk_io_led_orange);
		sk_tick_delay_ms(500);
	}
}
