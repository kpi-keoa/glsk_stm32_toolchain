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
#include <libopencm3/stm32/spi.h>
#include <stdint.h>
#include <stddef.h>



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


// Among all GL-SK peripherals, only external SPI flash (SST25VF016B) is connected via SPI
// PA5 - SPI1_SCK	- Alternate function AF5
// PB5 - SPI1_MOSI	- Alternate function AF5
// PB4 - SPI1_MISO	- Alternate function AF5
// PD7 - ~CS		- driven manually, use push-pull out with pullup (PULLUP IS IMPORTANT)
//
static void spi_init(void)
{
	// Setup GPIO

	// Our SST25VF016B memory chip has maximum clock frequency of 80 MHz, so set speed to high
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOD);
	// Pins directly assigned to SPI peripheral
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, 1 << 5);
	gpio_set_af(GPIOA, GPIO_AF5, 1 << 5);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, 1 << 5);

	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, (1 << 5) | (1 << 4));
	gpio_set_af(GPIOB, GPIO_AF5, (1 << 5) | (1 << 4));
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, (1 << 5) | (1 << 4));
	// CS Pin we drive manually
	gpio_set_output_options(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, 1 << 7);
	gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, 1 << 7);
	gpio_set(GPIOD, 1 << 7);

	rcc_periph_clock_enable(RCC_SPI1);
	// Disable SPI1 before configuring
	spi_disable(SPI1);		// not required here, SPI is disabled after Reset
	// SPI1 belongs to APB2 (86 MHz frequency)
	// We have /2, /4, /8, /16, /32, /64, /128 and /256 prescalers
	// Our SPI flash can work up to 50 MHz ... 80 MHz clock (depending on part marking)
	// But to be able to capture data with logic analyzer, which has maximum frequency of 24 MHz,
	// set spi baudrate to /32, which gives us 86/32 = 2.6 MHz SCLK frequency
	spi_set_baudrate_prescaler(SPI1, SPI_CR1_BR_FPCLK_DIV_32);
	// Our MCU is master and flash chip is slave
	spi_set_master_mode(SPI1);
	// We work in full duplex (simultaneous transmit and receive)
	spi_set_full_duplex_mode(SPI1);
	// Data frame format is 8 bit, not 16
	spi_set_dff_8bit(SPI1);
	// No CRC calculation is required
	spi_disable_crc(SPI1);
	// Our flash chip requires Most Significant Bit first (datasheet p. 5, Figure 3. SPI Protocol)
	spi_send_msb_first(SPI1);
	// Flash chip can work in Mode 0 (polarity 0, phase 0) and Mode 3 (polarity 1, phase 1)
	// But the chip inputs data on rising edge and outputs data on falling edge, so
	// MCU is better to read data on rising edge and output on falling edge -- select mode 3
	spi_set_clock_polarity_1(SPI1);
	spi_set_clock_phase_1(SPI1);
	// Set hardware control of NSS pin. Because disabling it can cause master to become slave
	// depending on NSS input state.
	// Normally NSS will be hard-wired to slave. But in our case we have other pin connected to
	// slave CS and should drive it manually
	spi_enable_ss_output(SPI1);

	// In this example we will work with flash without using interrupts
	// So simply enable spi peripheral
	spi_enable(SPI1);
}


// sets cs pin
static void cs_set(bool state)
{
	sk_pin_set(sk_io_spiflash_ce, state);
}


static void flash_tx(uint32_t len, const void *data)
{
	uint8_t *d = data;
	if ((!len) || (NULL == d))
		return;

	for (int32_t i = len - 1; i >= 0; i--) {
		spi_send(SPI1, d[i]);
		spi_read(SPI1);		// dummy read to provide delay
	}
}


static void flash_rx(uint32_t len, void *data)
{
	// Note:
	// Our spi chip uses Big Endian byte order, while MCU is Little Endian
	// This means 0xABCD will be represented as ABCD on MCU, and CDAB on spi chip
	// (spi chip expects higher address bytes to be transfered first)
	// This means we either need to declare our structures and arrays as big endian
	// with __attribute__(( scalar_storage_order("big-endian") )), which is more portable
	// but leads to heavier computations,
	// or solve this at transfer level, sending and receiving higher bytes first
	uint8_t *d = data;
	if ((!len) || (NULL == d))
		return;

	for (int32_t i = len - 1; i >= 0; i--) {
		spi_send(SPI1, 0);
		d[i] = spi_read(SPI1);
	}
}


struct __attribute__((packed, aligned(1))) 
       __attribute__(( scalar_storage_order("little-endian") ))
	   flash_jedec_id {
	// order matters here
	uint16_t device_id;
	uint8_t manufacturer;
};


int main(void)
{
	rcc_periph_clock_enable(RCC_GPIOE);		// lcd is connected to port E
	rcc_periph_clock_enable(RCC_GPIOD);		// DISCOVERY LEDS are connected to Port E
	glsk_pins_init(false);

	sk_pin_set(sk_io_led_green, true);	// turn on LED until everything is configured
	clock_init();
	sk_pin_set(sk_io_led_green, false);

	sk_tick_init(168000000ul / 10000ul, (2 << 2 | 0));

	cm_enable_interrupts();

	sk_lcd_init(&lcd);
	sk_lcd_set_backlight(&lcd, 200);

	spi_init();



	while (1) {
		// SPI communication demo
		sk_pin_set(sk_io_led_green, true);
		cs_set(0);		// assert enable signal

		const uint8_t cmd_jedec_id_get = 0x9F;
		flash_tx(1, &cmd_jedec_id_get);

		struct flash_jedec_id jedec_id = { 0 };
		flash_rx(sizeof(jedec_id), &jedec_id);

		cs_set(1);
		sk_pin_set(sk_io_led_green, false);

		char buffer[20];
		sk_lcd_cmd_setaddr(&lcd, 0x00, false);
		snprintf(buffer, sizeof(buffer), "Manufacturer:%Xh", (unsigned int)jedec_id.manufacturer);
		lcd_putstring(&lcd, buffer);

		sk_lcd_cmd_setaddr(&lcd, 0x40, false);
		snprintf(buffer, sizeof(buffer), "Serial:%Xh", (unsigned int)jedec_id.device_id);
		lcd_putstring(&lcd, buffer);

		sk_pin_toggle(sk_io_led_orange);
		sk_tick_delay_ms(500);
	}
}
