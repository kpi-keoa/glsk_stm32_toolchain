/**
 * libsk tick - provides support for system ticks
 * 
 * Ticks may be used for uptime accounting, providing delays and other means.
 */

#include "macro.h"
#include <stdint.h>
#include <stdbool.h>


/** 
 * System tick counter. Variable, which holds current tick value.
 * Initialized to 0 at startup and at initialization with :c:func:`sk_tick_init`.
 * Should not be used directly. Use setters/getters provided here instead.
 */
extern volatile uint32_t __sk_nticks;


/** Returns current system tick counter (:c:data:`__sk_nticks`) value */
inline sk_attr_alwaysinline uint32_t sk_tick_get_current(void)
{
	return __sk_nticks;
}


/**
 * Set current system tick counter (:c:data:`__sk_nticks`) value.
 * Note:
 * Normally this should never be called by user.
 * But is provided for some rare cases.
 */
inline sk_attr_alwaysinline void __sk_tick_set_current(uint32_t val)
{
	__sk_nticks = val;
}


/**
 * System tick increment callback.
 * This is called in ISR (usually Cortex SysTick handler isr) at regular time periods to
 * increment the current tick value
 */
inline sk_attr_alwaysinline void __sk_tick_inc_callback(void)
{
	__sk_nticks++;
}


/** 
 * Default SysTick ISR handler used by tick module.
 * This implementation simply calls :c:func:`__sk_tick_inc_callback` to increment tick
 * value.
 * User may override this handler to provide own implementation.
 */
void sk_attr_weak sys_tick_handler(void);


/**
 * Initialize tick internals for SysTick
 * @period: Tick period in clock cycles. Must be <= 0x7fffff8 in any case
 * @irq_priority: SysTick interrupt priority (as set by :c:func:`nvic_set_priority`)
 * @return: true on success, false on init failure
 *
 * Note:
 * Tries to use divider by 8 if applicable to extend possible period range.
 * Either any 24 bit, or 27 bit with lower 3 bits set to 0 (dividable by 8 with no remainder)
 * value must be used for period.
 */
bool sk_tick_init(uint32_t period, uint_fast8_t irq_priority);


/**
 * Returns current tick period in Hz.
 * Note:
 * The resulting value may be influenced by an integer division truncation.
 */
uint32_t sk_get_tick_rate_hz(void);


/**
 * Use tick counter to provide millisecond delays.
 * Note:
 * Puts MCU to sleep using WFI instruction regularly until the desired tick value is reached.
 */
void sk_tick_delay_ms(uint32_t ms);
