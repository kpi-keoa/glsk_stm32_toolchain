#include "lcd_hd44780.h"
#include "tick.h"
#include "intrinsics.h"
#include "printf.h"
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <stdio.h>
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


static void lcd_putstring(const char *str)
{
	for (char *cptr = str; *cptr != '\0'; cptr++)
		sk_lcd_putchar(&lcd, *cptr);
}


/**
 * Initialize ADC for getting temperature value from the analog sensor
 */
static void adc_temp_init(void)
{
	// * Sensor is connected to ADC12IN9 -- ADC1 and ADC2 (pin is common for them), input 9
	// * By opening STM32F407_Datasheet_(DS8626).pdf at page 50, we can see ADC12_IN9 is
	//   an additional function of PB1 pin
	// * So set PB1 to analog
	rcc_periph_clock_enable(RCC_GPIOB);
	// set ospeed to lowest to minimize noise and power use as application note recommends
	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, 1 << 1);
	// now set this pin to analog mode
	gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, 1 << 1);

	// setup ADC
	// * By looking at STM32F407_Datasheet_(DS8626).pdf page 133, we can see that for 3.3 V
	//   power, the ADC frequency fadc should be in range [0.6 MHz ... 36 MHz]
	//   with typical frequency around 30 MHz
	// * We're measuring temperature, which has slow-changing nature, but the input signal
	//   could have some fluctuations and noise. As we're not dealing with fast signal here,
	//   we're interested in long sampling time, which will serve us as a sort of analog
	//	 low-pass filter. And filter-out some noise.
	// * So let's set ADC frequency to some low value. Not the lowest possible 0.6 MHz, but
	//   with some margin (around 20 %) above the lowest value.

	// * Page 390 of Reference manual tells us:
	//	 ADCCLK, common to all ADCs is generated from the APB2 clock divided by a programmable
	//   prescaler that allows the ADC to work at f PCLK2 /2, /4, /6 or /8
	// * Our core (by default) is clocked from AHB bus at 16 MHz and we don't change the defaults
	//   in this example
	// * APB2 clock is derived from AHB and has its own dividing prescaler by /2, /4, /6, /8, /16
	// * But our GPIO is also clocked from APB2. And if we set APB2 divisor to maximum /16,
	//   it will also affect GPIO maximum frequency.
	// * So the clock scheme is as follows:
	//			 _____AHB_____
	//         /              \
	//	  [Core, ...]      __APB2__  with prescaler (/2, /4, /6, /8, /16)
	//				     /         \
	//				TIMER, ...	  ADC prescaler (/2, /4, /6 or /8)
	//								|
	//							   ADC
	// * So if we set ADC frequency to some low value. Not the lowest possible 0.6 MHz, but
	//   with some margin (around 20 %) above the lowest value, we could have some alternative
	//   lowest frequency variants:
	//       16 MHz / 24 = 0.66 MHz    ( good, divide by /4 and by /6 giving us division by /24 )
	//		 16 MHz / 32 = 0.5 MHz   !!!! below allowed, so not an option !!!!
	// 	     16 MHz / 16 = 1 MHz       ( also good )
	// * RESULT: let's set to 1 MHz with /2 ABP2 prescaler and /8 ADC prescaler

	// * We have ADC12 pin, this means ADC1 and ADC2. Any can be chosen. Let's work with ADC1

	// Set APB2 divisor to /2
	rcc_set_ppre2(RCC_CFGR_PPRE_DIV_2);
	// Enable ADC clock
	rcc_periph_clock_enable(RCC_ADC1);

	// Set ADC prescaler to /8 (bits ADCPRE in ADC_CCR)
	adc_set_clk_prescale(ADC_CCR_ADCPRE_BY8);

	// Set ADC resolution to 12 bit
	adc_set_resolution(ADC1, ADC_CR1_RES_12BIT);
	// Set left-to-right data alignment in adc result register 
	adc_set_right_aligned(ADC1);

	// * We will convert 16 times using ADC groups feature, then calculate average value
	//   to further reduce noise and jitter. Set up these groups
	// * Set sampling time. We will sample for 480 fadc cycles, which gives us sampling
	//   frequency of Fs = fadc / 480 = 1 MHz / 480 = 2083.33 Hz
	// * So, set all 16 regular group sampling times to 480 cycles (SMPx bits in ADC_SMPRx)
	uint8_t channels[16];
	for (int i = 0; i < sk_arr_len(channels); i++) {
		adc_set_sample_time(ADC1, i, ADC_SMPR_SMP_480CYC);
		channels[i] = 9;	// set each element of group to channel 1
	}
	adc_set_regular_sequence(ADC1, sk_arr_len(channels), channels);

	// Configure End of Conversion (EOC) flag to be set after each channel in group
	// is converted. This will raise interrupt, where we read the conversion value
	adc_eoc_after_each(ADC1);

	// Enable scan mode to convert a group of channels
	adc_enable_scan_mode(ADC1);

	// Configure to perform a conversion of group of channels and stop
	adc_set_single_conversion_mode(ADC1);

	adc_enable_eoc_interrupt(ADC1);
	adc_enable_overrun_interrupt(ADC1);

	// Set ADC interrupts in NVIC
	nvic_set_priority(NVIC_ADC_IRQ, 10);
	nvic_enable_irq(NVIC_ADC_IRQ);

	// Enable ADC1. And wait some time to stabilize and calibrate itself
	adc_power_on(ADC1);
	sk_tick_delay_ms(10);
}

static volatile uint16_t __adc_avgval = 0;

void adc_isr(void)
{
	static volatile uint32_t sum = 0;
	static volatile uint32_t cnt = 0;

	if (adc_get_overrun_flag(ADC1)) {
		// means we got to interrupt because of overrun flag
		// we have not read all data and overrun occured -> reset everything
		sum = cnt = 0;
		adc_clear_flag(ADC1, ADC_SR_OVR);	// reset flag to avoid infinite interrupting
		return;
	}

	// otherwise we got here because one of channels was converted
	sum += adc_read_regular(ADC1) & 0x00000FFF;
	cnt++;

	if (cnt >= 16) {		// all sequence was read
		__adc_avgval = sum / cnt;
		sum = cnt = 0;
		adc_clear_flag(ADC1, ADC_SR_STRT);	// clear regular channel
	}

	adc_clear_flag(ADC1, ADC_SR_EOC);	// clear end of conversion flag not to cycle
}


static uint16_t adc_acquire(void)
{
	// start conversion
	adc_start_conversion_regular(ADC1);
	
	// wait for conversion to start
	while (! adc_get_flag(ADC1, ADC_SR_STRT)) {
		__WFI();
	}
	// sleep while adc has not finished coverting all channels in group
	while (adc_get_flag(ADC1, ADC_SR_STRT)) {
		__WFI();
	}

	return __adc_avgval;	// converted value after averaging in ISR
}


static inline float adc_temp_convert(uint16_t adcval)
{
	return 97.31 - 0.04039 * adcval;
}



int main(void)
{
	rcc_periph_clock_enable(RCC_GPIOE);		// LCD is connected to portE
	glsk_pins_init(false);

	sk_tick_init(16000000ul / 10000ul, 2);
	cm_enable_interrupts();

	adc_temp_init();

	sk_lcd_init(&lcd);
	sk_lcd_set_backlight(&lcd, 0xFF);
	lcd_putstring("    ADC demo");
	// TODO: implement printf-like interface with special chars (\r \n \t ...)
	sk_lcd_cmd_setaddr(&lcd, 0x40, false);	// 2nd line begins from addr 0x40

	while (1) {
		uint16_t adcval = adc_acquire();
		float temp = adc_temp_convert(adcval);
		sk_lcd_cmd_setaddr(&lcd, 0x40, false);
		char tmp[20], buffer[20];
		snprintf(tmp, sk_arr_len(tmp), "%5.1f", temp);
		snprintf(buffer, sk_arr_len(buffer), "T=%-5s°C A=%u", tmp, (unsigned int)adcval);
		lcd_putstring(buffer);
	}
}
