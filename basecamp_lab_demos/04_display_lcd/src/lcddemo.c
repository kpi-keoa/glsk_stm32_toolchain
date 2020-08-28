#include "lcd_hd44780.h"
#include "tick.h"
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>



static void init_bkl_pwm(void)		// dirty code, needs refactoring
{
	nvic_set_priority(NVIC_EXTI0_IRQ, 2 << 2 | 3);
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO0);		// pulldown is external
	rcc_periph_clock_enable(RCC_SYSCFG);
	exti_select_source(EXTI0, GPIOA);
	exti_set_trigger(EXTI0, EXTI_TRIGGER_RISING);
	exti_enable_request(EXTI0);
	exti_reset_request(EXTI0);
	nvic_enable_irq(NVIC_EXTI0_IRQ);


	gpio_set_output_options(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, 1 << 9);
	gpio_set_af(GPIOE, GPIO_AF1, 1 << 9);
	gpio_mode_setup(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, 1 << 9);
	
	rcc_periph_clock_enable(RCC_TIM1);

	timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM1, 16000000 / (256*1000));
	timer_enable_preload(TIM1);
	timer_set_period(TIM1, 255);
	//timer_continuous_mode(TIM1);
	timer_set_oc_mode(TIM1, TIM_OC1, TIM_OCM_PWM1);
	timer_enable_oc_output(TIM1, TIM_OC1);
	timer_enable_break_main_output(TIM1);
	timer_set_oc_value(TIM1, TIM_OC1, 0);
	timer_enable_counter(TIM1);
}


static void timer1_set_pwm_backlight(uint8_t val)
{
	timer_set_oc_value(TIM1, TIM_OC1, val);	// we have TIM1_CH1 connected to backlight
}


static void lcd_putstring(struct sk_lcd *lcd, const char *str)	// dummy example
{
	char *ptr = str;
	while (*ptr != '\0') {
		sk_lcd_putchar(lcd, *ptr);
		ptr++;
	}
}


static struct sk_lcd lcd = {
	.pin_group_data = &sk_io_lcd_data,
	.pin_rs = &sk_io_lcd_rs,
	.pin_en = &sk_io_lcd_en,
	.pin_rw = &sk_io_lcd_rw,
	//.pin_bkl = &sk_io_lcd_bkl,
	.set_backlight_func = &timer1_set_pwm_backlight,
	.delay_func_us = NULL,
	.delay_func_ms = &sk_tick_delay_ms,
	.is4bitinterface = true,
	.charmap_func = &sk_lcd_charmap_rus_cp1251
};


void exti0_isr(void)		// dirty code, needs refactoring
{
	static uint8_t bkl = 0;
	sk_lcd_set_backlight(&lcd, bkl-1u);
	bkl += 16u;
	exti_reset_request(EXTI0);
}


int main(void)
{
    rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOE);		// lcd is connected to port E
	glsk_pins_init(false);
	sk_pin_group_set(sk_io_lcd_data, 0x00);
	sk_pin_set(sk_io_led_orange, true);

	sk_tick_init(16000000ul / 10000ul, 2);
	cm_enable_interrupts();

	init_bkl_pwm();
	
	sk_pin_set(sk_io_led_orange, false);

	sk_lcd_init(&lcd);
	sk_lcd_cmd_onoffctl(&lcd, true, false, false);	// display on, cursor off, blinking off
	sk_lcd_set_backlight(&lcd, 200);


	lcd_putstring(&lcd, "Здравствуй, мир!");
	// TODO: implement printf-like interface with special chars (\r \n \t ...)
	sk_lcd_cmd_setaddr(&lcd, 0x40, false);	// 2nd line begins from addr 0x40
	lcd_putstring(&lcd, "Hello, world!");

	sk_pin_set(sk_io_led_orange, true);

	bool isdirright = true;
    while (1) {
		// dumb code for logic analyzer to test levels
		for (int i = 0; i < 16; i++) {
			sk_lcd_cmd_shift(&lcd, true, isdirright);
			if (!isdirright) sk_tick_delay_ms(200);
		}
		if (!isdirright) sk_tick_delay_ms(3000);
		isdirright = !isdirright;	// swap shift direction
		sk_pin_set(sk_io_led_orange, isdirright);
    }
}
