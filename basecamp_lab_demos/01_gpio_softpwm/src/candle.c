#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>

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


/** Constants used in LCG generator. Names A, C, M are well-known definitions */
static const uint32_t LCG_CONST_A = 8121ul,
					  LCG_CONST_C = 28411ul,
					  LCG_CONST_M = 134456ul;

/** Stores last LCG generated value */
static uint32_t __lcgrand = 0;


/**
 * Performs initialization of LCG generator with provided initial value
 * @init_val: initial value
 */
inline void lcgrand_init(uint32_t init_val)
{
	__lcgrand = init_val;
}


/**
 * Generate pseudo-random number using LCG (Linear Congruential Generator) algorithm
 */
uint32_t lcgrand(void)
{
	// try to be context switch safe, so return generated value, not stored one
	uint32_t next = (__lcgrand * LCG_CONST_A + LCG_CONST_C) % LCG_CONST_M;
	__lcgrand = next;
	return next;
}

/**
 * Array mapping incremental randoms to imitated candle brigtness with level probabilities:
 * level |  0..4   5..14  15
 * prob  |  0.2%   4.9%   50%
 * All constants are mapped to 32-bit generator max 2169777276
 * (LCG with a=8121, c=28411, m=134456)
 */
static const uint16_t candle_probarr[] = {
	131u,
	262u,
	393u,
	524u,
	655u,
	3867u,
	7078u,
	10289u,
	13500u,
	16711u,
	19923u,
	23134u,
	26345u,
	29556u,
	32768u
};

/**
 * Return imitated candle brightness level
 */
static uint_fast8_t get_candle_brightness_lvl(void)
{
	uint_fast16_t randval = lcgrand() % 0xFFFF;
	// Here we trace through array finding the first element, for which
    // our generated random value is smaller than element value.
	// And index of that element will be our brightness level
	uint_fast8_t i = 0;
	for (; i < sizeof(candle_probarr)/sizeof(*candle_probarr); i++) {
		if (randval < candle_probarr[i])
			break;
	}
	return i;
}


// Note: this function has glitch possibility
static void softpwm_one_cycle(uint32_t period, uint32_t duty, uint32_t gpioport, uint16_t gpiopins)
{
	gpio_set(gpioport, gpiopins);
	if (duty > period)
		return;
	softdelay(duty);
	gpio_clear(gpioport, gpiopins);
	softdelay(period - duty);
}


const uint32_t PWM_PERIOD_CYCLES = 500;
const uint32_t PWM_REPEAT_CYCLES = 30000;

static void softpwm_candle_set(uint32_t gpioport, uint16_t gpiopins)
{
	uint32_t lvl = get_candle_brightness_lvl();
	for (uint32_t i = 0; i < PWM_REPEAT_CYCLES; i++) {
		softpwm_one_cycle(PWM_PERIOD_CYCLES, (PWM_PERIOD_CYCLES * lvl) / 15, gpioport, gpiopins);
	}
}

int main(void)
{
    rcc_periph_clock_enable(RCC_GPIOD);
    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12 | GPIO13 | GPIO14);
	gpio_clear(GPIOD, GPIO12);
    gpio_set(GPIOD, GPIO13 | GPIO14);
    while (1) {
		softpwm_candle_set(GPIOD, GPIO13 | GPIO14);
    }
}
