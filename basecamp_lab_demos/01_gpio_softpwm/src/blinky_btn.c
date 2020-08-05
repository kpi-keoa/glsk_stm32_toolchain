#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Simple illustrative softdelay (inefficient though)
 * @N: abstract number of cycles. Not CPU cycles at all
 *
 * Note:
 * This way of delaying is really really inefficient:
 * What it does is makes CPU spend time in empty loop consuming energy
 * and heating up the atmosphere. Whenever delay is required, there are better ways
 * of achieving it. CPU can sleep saving some energy or do some useful calculations while waiting.
 * However, softdelays are sometimes used for achieving ultra-small delays (i.e. in bit-bang). But
 * in that case, softdelays are fine-tuned against CPU frequency and other stuff for fine-grained
 * and more deterministic delay times.
 */
void softdelay(volatile uint32_t N)
{
    while (N--);
    // or: while (N--) __asm__("nop");
}

bool pin_read(uint32_t gpioport, uint16_t pin)
{
	// assuming buttons are ACTIVE LOW
	if (!(gpio_port_read(gpioport) & pin)) {
		// todo: debounce me
		return true;
	}
	return false;
}


int main(void)
{
    rcc_periph_clock_enable(RCC_GPIOD);		// leds are here
	rcc_periph_clock_enable(RCC_GPIOC);		// glsk buttons are here
    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12);
	gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO9 | GPIO11);
    gpio_set(GPIOD, GPIO12);
	uint32_t delay_time = 800000,
			 delay_delta = 100000;

    while (1) {
        gpio_toggle(GPIOD, GPIO12);
        softdelay(delay_time);

		// polling. Interrupts will be shown in further lessons
		if (pin_read(GPIOC, GPIO9))		// button up (SWT3)
			delay_time += delay_delta;
		if (pin_read(GPIOC, 1 << 11))
			delay_time -= delay_delta;
    }
}
