#include "lcd_hd44780.h"
#include "pin.h"
#include "tick.h"
#include "macro.h"
#include <libprintf/printf.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>



static struct sk_lcd lcd = {
	.pin_group_data = &sk_io_lcd_data,
	.pin_rs = &sk_io_lcd_rs,
	.pin_en = &sk_io_lcd_en,
	.pin_rw = &sk_io_lcd_rw,
	.pin_bkl = &sk_io_lcd_bkl,
	.set_backlight_func = NULL,
	.delay_func_us = NULL,
	.delay_func_ms = &sk_tick_delay_ms,
	.is4bitinterface = true,
	.charmap_func = &sk_lcd_charmap_rus_cp1251
};


static void lcd_putstring(struct sk_lcd *lcd, const char *str)	// dummy example
{
	char *ptr = str;
	while (*ptr != '\0') {
		sk_lcd_putchar(lcd, *ptr);
		ptr++;
	}
}


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


// PB6 - I2C1 SCL : Alternate function AF4 : (4.7 k pullup on STM32F4DISCOVERY board)
// PB9 - I2C1 SDA : Alternate function AF4 : (4.7 k pullup on STM32F4DISCOVERY board)
// 
static void i2c_init(uint32_t i2c)
{
	// Setup GPIO
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, (1 << 6) | (1 << 9));
	gpio_set_af(GPIOB, GPIO_AF4, (1 << 6) | (1 << 9));
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, (1 << 6) | (1 << 9));

	rcc_periph_clock_enable((i2c == I2C1) ? RCC_I2C1 : RCC_I2C2);
	i2c_peripheral_disable(i2c);

	// set input clock frequency (which comes from APB1)
	i2c_set_clock_frequency(i2c, rcc_apb1_frequency / 1000000);
	
	i2c_set_standard_mode(i2c);		// set communication frequency to Sm (100 kHz)
	//i2c_set_dutycycle(i2c, I2C_CCR_DUTY_DIV2);		// Tlow / Thigh = 2 (relates only to Fm)

	// CCR = F_PCLK1 / (2 * F_i2c) = 42 MHz / (2 * 100 kHz) = 42e6 / (2 * 100e3) = 210
	i2c_set_ccr(i2c, (rcc_apb1_frequency / (2ul * 100000ul)));	// 100 kHz communication speed

	// Trise = 1 + Tmax / T_PCLK1 = 1 + F_PCLK1 * Tmax, where Tmax is given is I2C specs
	// for 100 kHz, Tmaz = 1000 ns = 1000e-9 s. => Trise = 1 + F_PCLK / 1e6
	i2c_set_trise(i2c, (1ul + rcc_apb1_frequency/1000000ul));

	i2c_peripheral_enable(i2c);
}


static float veclen(float x, float y, float z)
{
	return sqrtf(x*x + y*y + z*z);
}



static void simplei2c_communicate_loop(uint32_t i2c)
{
	// slave addr for magnetic sensor is 0x1E (bits 7...1)
	uint8_t saddr = 0x1E;
	
	uint8_t cmd_set_contigious[] = {
		0x22,	// register 22h | (0 << 7)
		0x00
	};
	i2c_transfer7(i2c, saddr, cmd_set_contigious, sk_arr_len(cmd_set_contigious), 0, 0);

	while (1) {
		int16_t x = 0, y = 0, z = 0;
		uint8_t var[6] = {0};
		uint8_t cmd_read_xyz = 0x28 | (1 << 7);		// auto increment
		i2c_transfer7(i2c, saddr, &cmd_read_xyz, 1, var, sk_arr_len(var));
		x = *(int16_t *)(var);
		y = *(int16_t *)(&var[2]);
		z = *(int16_t *)(&var[4]);

		float abs = veclen(x, y, z);

		char buffer[20];
		sk_lcd_cmd_setaddr(&lcd, 0x00, false);
		snprintf(buffer, sizeof(buffer), "%-6dx %-6dy", (int)x, (int)y);
		lcd_putstring(&lcd, buffer);
		sk_lcd_cmd_setaddr(&lcd, 0x40, false);
		snprintf(buffer, sizeof(buffer), "%-6dz %-5dabs", (int)z, (int)abs);
		lcd_putstring(&lcd, buffer);
	}
}


int main(void)
{
	rcc_periph_clock_enable(RCC_GPIOE);		// lcd is connected to port E
	rcc_periph_clock_enable(RCC_GPIOD);		// DISCOVERY LEDS are connected to Port E
	glsk_pins_init(false);

	sk_pin_set(sk_io_led_green, true);	// turn on LED until everything is configured
	clock_init();
	i2c_init(I2C1);
	sk_pin_set(sk_io_led_green, false);

	sk_tick_init(168000000ul / 10000ul, (2 << 2 | 0));

	cm_enable_interrupts();

	sk_tick_delay_ms(1000);
	for (int i = 0; i < 5; i++)
		sk_lcd_init(&lcd);
	sk_lcd_set_backlight(&lcd, 200);

	simplei2c_communicate_loop(I2C1);
}
