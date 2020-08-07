#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <stdint.h>


void softdelay(uint32_t N)
{
	while (N--) __asm__("nop");
}


// EXTI0 ISR (Interrupt Service Routine)
void exti0_isr(void)
{
	// simplest debouncing
	// (!) please, try commenting it out to see how bouncing looks like for real
	softdelay(200);		// using softdelays in IRQ is really really bad. IRQs must be fast
	if (gpio_get(GPIOA, GPIO0))		// check that our button is still pressed after some time
		gpio_toggle(GPIOD, GPIO12);

	// This one removes interrupt request. Try also commenting it out to see how
	// the same interrupt is triggered again and again if not being reset
	exti_reset_request(EXTI0);
}


int main(void)
{
	// Configure and turn on LED
	// (!) Good practice. Turn on before init and turn off after everything is initialized
	// so that we know if something goes wrong during init
    rcc_periph_clock_enable(RCC_GPIOD);
    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12);
	gpio_set(GPIOD, GPIO12);

	// User push button (ACTIVE LOW) on Discovery board is connected to PA0
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO0);		// pulldown is external


	// Configure interrupts

	// Set priority grouping. See p.228 Programming Manual for more
	scb_set_priority_grouping(SCB_AIRCR_PRIGROUP_GROUP4_SUB4);	// default

	// Set priority for EXTI0. 
	const uint8_t group = 2;
	const uint8_t subgroup = 0;
	nvic_set_priority(NVIC_EXTI0_IRQ, (group << 2) | subgroup);


	// Enable EXTI port clock
	// This is not obvious, but selected port is stored in syscfg registers
	rcc_periph_clock_enable(RCC_SYSCFG);

	exti_select_source(EXTI0, GPIOA);
	exti_set_trigger(EXTI0, EXTI_TRIGGER_RISING);	// ACTIVE HIGH, so trigger on 0->1 transition
	exti_enable_request(EXTI0);

	// Important: if we don't reset request before interrupt is enabled at NVIC
	//            we will have first "phantom" interrupt request despite no button had been pressed
	exti_reset_request(EXTI0);

	// Now finally enable it
	nvic_enable_irq(NVIC_EXTI0_IRQ);

	// Globally enable interrupts. Just to show how it's done
	// Find docs at: 
	//      http://libopencm3.org/docs/latest/stm32f4/html/group__CM3__cortex__defines.html
	cm_enable_interrupts();

    gpio_toggle(GPIOD, GPIO12);		// this should normally turn led off


    while (1) {
		// everything is done in interrupt
    }
}
