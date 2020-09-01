#include "pin.h"
#include <libopencm3/stm32/rcc.h>

/**
 * Densification helper (private)
 * Example:
 *     mask = 0b1010000011110010
 *   sparse = 0b1010101010101010
 *   result = 0b1 1     1010  1
 */
static uint16_t group_densify(uint16_t mask, uint16_t sparse)
{
	uint16_t ret = 0;
	int idx = 0;
	for (int i = 0; i < 16; i++) {
		if (mask & (1 << i)) {
			ret |= sparse & (1 << i) ? (1 << idx) : 0;
			idx++;
		}
	}
	return ret;
}


/**
 * Sparsification helper (private)
 * Example:
 *     mask = 0b1010000011110010
 *    dense = 0b1 1     1010  1
 *   result = 0b1010000010100010
 */
static uint16_t group_sparsify(uint16_t mask, uint16_t dense)
{
	uint16_t ret = 0;
	int idx = 0;
	for (int i = 0; i < 16; i++) {
		if (mask & (1 << i)) {
			ret |= dense & (1 << idx) ? (1 << i) : 0;
			idx++;
		}
	}
	return ret;
}


bool sk_pin_read(sk_pin pin)
{
	bool ret = gpio_port_read(sk_pin_port_to_gpio(pin.port)) & (1 << pin.pin);
	return ret ^ pin.isinverse;
}


void sk_pin_set(sk_pin pin, bool value)
{
	if (value ^ pin.isinverse)
		gpio_set(sk_pin_port_to_gpio(pin.port), (1 << pin.pin));
	else
		gpio_clear(sk_pin_port_to_gpio(pin.port), (1 << pin.pin));
}


void sk_pin_toggle(sk_pin pin)
{
	gpio_toggle(sk_pin_port_to_gpio(pin.port), (1 << pin.pin));
}


uint16_t sk_pin_group_read(sk_pin_group group)
{
	uint16_t val = gpio_port_read(sk_pin_port_to_gpio(group.port));
	val ^= group.inversions;
	return group_densify(group.pins, val);
}


void sk_pin_group_set(sk_pin_group group, uint16_t values)
{
	// We want to change only pins we use in group, and don't touch the others
	// We also want to manipulate the output register, not what's on port input at the moment,
    // so gpio_port_read() does not suit. Use GPIO ODR register directly
	values = group_sparsify(group.pins, values);
	values ^= group.inversions;
	volatile uint32_t *odr = &GPIO_ODR(sk_pin_port_to_gpio(group.port));
	uint32_t pval = *odr;
	pval &= ~((uint32_t)(group.pins));	// reset all pins in account
	pval |= values;			// set all pins in account to our values
	*odr = pval;
}


void sk_pin_group_toggle(sk_pin_group group, uint16_t values)
{
	values = group_sparsify(group.pins, values);
	volatile uint32_t *odr = &GPIO_ODR(sk_pin_port_to_gpio(group.port));
	*odr ^= values;
}


#if defined(SK_USE_GLSK_DEFINITIONS) && SK_USE_GLSK_DEFINITIONS
// some STM32F4DISCOVERY pins
const sk_pin sk_io_led_orange 	= { .port=SK_PORTD, .pin=13, .isinverse=false };
const sk_pin sk_io_led_red    	= { .port=SK_PORTD, .pin=14, .isinverse=false };
const sk_pin sk_io_led_green  	= { .port=SK_PORTD, .pin=12, .isinverse=false };
const sk_pin sk_io_led_blue   	= { .port=SK_PORTD, .pin=15, .isinverse=false };
const sk_pin sk_io_btn_usr    	= { .port=SK_PORTA, .pin=0,  .isinverse=false };
// some GL-SK pins
const sk_pin sk_io_btn_right  	= { .port=SK_PORTC, .pin=11, .isinverse=true  };
const sk_pin sk_io_btn_mid    	= { .port=SK_PORTA, .pin=15, .isinverse=true  };
const sk_pin sk_io_btn_left   	= { .port=SK_PORTC, .pin=9,  .isinverse=true  };
const sk_pin sk_io_btn_up     	= { .port=SK_PORTC, .pin=6,  .isinverse=true  };
const sk_pin sk_io_btn_down   	= { .port=SK_PORTC, .pin=8,  .isinverse=true  };
const sk_pin sk_io_eth_led    	= { .port=SK_PORTB, .pin=0,  .isinverse=true  };
const sk_pin sk_io_lcd_bkl    	= { .port=SK_PORTE, .pin=9,  .isinverse=false };
const sk_pin sk_io_lcd_rs     	= { .port=SK_PORTE, .pin=7,  .isinverse=false };
const sk_pin sk_io_lcd_rw     	= { .port=SK_PORTE, .pin=10, .isinverse=false };
const sk_pin sk_io_lcd_en     	= { .port=SK_PORTE, .pin=11, .isinverse=false };
const sk_pin sk_io_spiflash_ce  = { .port=SK_PORTD, .pin=7,  .isinverse=false };
// GL-SK LCD 4-bit data interface pin group
const sk_pin_group sk_io_lcd_data = {
	.port = SK_PORTE,
	.pins = (1 << 15) | (1 << 14) | (1 << 13) | (1 << 12),
	.inversions = 0
};


#if !(SK_USE_SIZE_OPTIMIZATIONS)

static inline void rcc_set_state(enum sk_port portnum, bool state)
{
	uint32_t rccunit = (0x30 << 5) | portnum;
	if (state)
		rcc_periph_clock_enable(rccunit);
	else
		rcc_periph_clock_disable(rccunit);
}


void glsk_pins_init(const bool set_all)
{

	// Initialize all pins to analog, low-speed, af0, push-pull, no-pullup
	// Analog mode will allow for lower power consumption
	for (int i = SK_PORTA; set_all && (i <= SK_PORTH); i++) {
		rcc_set_state(i, true);
		uint32_t port = sk_pin_port_to_gpio(i);
		gpio_mode_setup(port, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, 0xffff);
		gpio_set_output_options(port, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, 0xffff);
		gpio_set_af(port, 0, 0xffff);
		rcc_set_state(i, false);
	}

	sk_pin out_pins[] = {
		sk_io_led_orange,
		sk_io_led_red,
		sk_io_led_green,
		sk_io_led_blue,
		sk_io_eth_led,
		sk_io_lcd_bkl,
		sk_io_lcd_rs,
		sk_io_lcd_rw,
		sk_io_lcd_en,
		sk_io_spiflash_ce
	};

	// set all outputs to out, push-pull, no pullup
	sk_arr_foreach(pin, out_pins) {
		rcc_set_state(pin.port, true);
		uint32_t port = sk_pin_port_to_gpio(pin.port);
		gpio_mode_setup(port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, (1 << pin.pin));
		sk_pin_set(pin, false);
	}

	rcc_set_state(sk_io_lcd_data.port, true);
	gpio_mode_setup(sk_pin_port_to_gpio(sk_io_lcd_data.port), GPIO_MODE_OUTPUT,
				    GPIO_PUPD_NONE, sk_io_lcd_data.pins);

	sk_pin in_pins[] = {
		sk_io_btn_usr,
		sk_io_btn_right,
		sk_io_btn_left,
		sk_io_btn_mid,
		sk_io_btn_up,
		sk_io_btn_down
	};

	// set all inputs to in, no pullup
	sk_arr_foreach(pin, in_pins) {
		rcc_set_state(pin.port, true);
		uint32_t port = sk_pin_port_to_gpio(pin.port);
		gpio_mode_setup(port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, (1 << pin.pin));
	}
}
#endif
#endif
