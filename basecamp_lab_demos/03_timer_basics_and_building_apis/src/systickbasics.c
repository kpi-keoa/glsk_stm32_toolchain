#include "pin.h"
#include "macro.h"
#include <libopencm3/cm3/sync.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <libopencm3/cm3/sync.h>


// WFI intrinsic
inline __attribute__ ((always_inline)) void __WFI(void)
{
	__asm__ volatile ("wfi");
}


volatile uint32_t __nticks = 0;


inline sk_attr_alwaysinline void __tick_inc_callback(void)
{
	__nticks++;
}


inline sk_attr_alwaysinline uint32_t tick_get_current(void)
{
	return __nticks;
}


const uint32_t TICK_RATE_HZ = 10000ul;


// SysTick ISR
void __attribute__ ((weak)) sys_tick_handler(void)
{
	__tick_inc_callback();
}


// basic init
void sys_tick_init(void)
{
	// disable just in case we're trying to reconfigure
	// while the SysTick is running
	systick_counter_disable();

	// Work @ Fcpu frequency. Assuming 16 MHz
	// Change to STK_CSR_CLKSOURCE_AHB_DIV8 for Fcpu/8
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);

	systick_interrupt_enable();

	// tick frequency will be 10 kHz (100 us tick rate)
	uint32_t period = 16000000lu / 10000lu;
	systick_set_reload(period);

	// load value
	STK_CVR = period;

	nvic_set_priority(NVIC_SYSTICK_IRQ, 2);
	nvic_enable_irq(NVIC_SYSTICK_IRQ);

	__nticks = 0;

	systick_counter_enable();
}


static void delay_ms_systick(uint32_t ms)
{
	uint32_t cur = tick_get_current();
	uint32_t delta = (TICK_RATE_HZ / 1000) * ms;
	uint32_t next = cur + delta;

	// overflow not taken into account here
	while (tick_get_current() <= next) {
		__WFI();
	}
}


int main(void)
{
    rcc_periph_clock_enable(RCC_GPIOD);
    //gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12);
	glsk_pins_init(true);
	sk_pin_set(sk_io_led_orange, true);

	sys_tick_init();
	cm_enable_interrupts();

    while (1) {
		sk_pin_toggle(sk_io_led_orange);
        //for (int i = 0; i < 15; i++) delay_us(65535);
		delay_ms_systick(500);
    }
}
