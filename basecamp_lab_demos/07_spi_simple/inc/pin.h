#pragma once
/**
 * libsk pin abstraction layer.
 */

#include "config.h"
#include "macro.h"
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>
#include <stdbool.h>


/**
 * GPIO ports definition (port A .. K).
 * Ports J and K are unavailable when :c:macro:`SK_USE_SIZE_OPTIMIZATIONS` is set
 * to fit into :c:type:`sk_pin` 8-bit field
 */
enum sk_port {
	SK_PORTA = 0,
	SK_PORTB = 1,
	SK_PORTC = 2,
	SK_PORTD = 3,
	SK_PORTE = 4,
	SK_PORTF = 5,
	SK_PORTG = 6,
	SK_PORTH = 7,
#if !(SK_USE_SIZE_OPTIMIZATIONS)
	/** Warning: Port I is used on some STM32F40xx packages */
	SK_PORTI = 8,
	SK_PORTJ = 9,
	SK_PORTK = 10
#endif
};


/**
 * Represents separate GPIO pin
 */
struct sk_attr_pack(1) sk_pin {
#if SK_USE_SIZE_OPTIMIZATIONS
	/** Port value as :c:type:`sk_port` */
	uint8_t port      : 3;	// use shorter variant for packing into 1 byte
	/** Pin number (0 .. 15) */
	uint8_t pin       : 4;  // we have at maximum 16 pins in port -> 4 bits
	/** Set to true if port input or output should be inverted */
	uint8_t isinverse : 1;	// support value inversionm, it is useful dealing with connections
#else
	/** Port value as :c:type:`sk_port` */
	uint16_t port       : 4;
	/** Pin number (0 .. 15) */
	uint16_t pin        : 4;
	/** Set to true if port input or output should be inverted */
	uint16_t isinverse  : 1;
	/** Packing residue reserved for furter use */
	uint16_t __reserved : 7;  // 7 bits left. Reserve them for further use
#endif
};


/**
 * Represents group of pins residing in the same GPIO port.
 * Used as an optimization to allow reading/writing them all at once.
 */
struct sk_attr_pack(1) sk_pin_group {
#if SK_USE_SIZE_OPTIMIZATIONS
	/** Port value as :c:type:`sk_port` */
	uint16_t port       : 3;
	/** Packing residue reserved for furter use */
	uint16_t __reserved : 13;
#else
	/** Port value as :c:type:`sk_port` */
	uint16_t port       : 4;
	/** Packing residue reserved for furter use */
	uint16_t __reserved : 12;
#endif
	/** 16-bit value where each bit represents corresponding pin in a port */
	uint16_t pins;
	/** 16-bit mask where each bit==1 represents inversion in :c:member:`sk_pin_group.pins` */
	uint16_t inversions;
};


typedef struct sk_pin sk_pin;
typedef struct sk_pin_group sk_pin_group;



/** Map sk_port definitions to libopencm3 GPIOx. Intended mainly for private use */
inline sk_attr_alwaysinline uint32_t sk_pin_port_to_gpio(enum sk_port port)
{
	// faster and more concise than building 1:1 map
	// benefit from memory locations of GPIO registers
	
	return GPIO_PORT_A_BASE + (GPIO_PORT_B_BASE - GPIO_PORT_A_BASE) * port;
}


// we pass by value (not by pointer) because this way compiler can optimize out
// variables from memory

/**
 * Read pin input and return its value
 * @pin: pin to read
 * @return: boolean value of pin input
 *
 * Note:
 * For the means of improved speed, directly reads port input register and performs no checks
 * that the pin is really set to input.
 * The user is responsible for checking pin configuration validity.
 * 
 * Inversion is taken into account as specified in :c:type:`sk_pin`
 */
bool sk_pin_read(sk_pin pin);


/**
 * Set pin output level to specified value
 *
 * Note:
 * Does not check if pin is set to output, to improve speed.
 * The user is responsible for providing a correct pin as an argument.
 *
 * Inversion is taken into account as specified in :c:type:`sk_pin`
 */
void sk_pin_set(sk_pin pin, bool value);


/**
 * Toggle pin output level to the opposite value
 *
 * Note:
 * Does not check if pin is set to output, for speed improvement.
 * The user is responsible for providing a correct pin.
 *
 * Inversion is taken into account as specified in :c:type:`sk_pin`
 */
void sk_pin_toggle(sk_pin pin);


/**
 * Read group of pins and return collected value.
 * @group: pin group (:c:type:`sk_pin_group`)
 * @return: densified value read from specified :c:member:`sk_pin_group.pins`
 * 
 * Only the pins specified in :c:type:`sk_pin_group` are collected in the resulting value applying
 * densification.
 * If :c:member:`sk_pin_group.pins` are specified, for example as 0b0010100100001100,
 * and GPIO has value 0bABCDEFGHIJKLMNOP, the resulting (collected) value will be
 * 0bCEHMN.
 */
uint16_t sk_pin_group_read(sk_pin_group group);


/**
 * Set group of pins to the provided (densified) value
 * @group: pin group (:c:type:`sk_pin_group`)
 * @values: densified value specifying which pins to set
 * 
 * Only the pins specified in :c:type:`sk_pin_group` are affected by set operation.
 * If :c:member:`sk_pin_group.pins` are specified, for example as 0b0101000000000011,
 * and `values` is set to 0b000011110000ABCD, the resulting GPIO value will be
 * 0b0A0B0000000000CD.
 *
 * Note:
 * Access to the corresponding GPIO registers is not atomic.
 */
void sk_pin_group_set(sk_pin_group group, uint16_t values);

/**
 * Toggle group of pins to the opposite values
 * @group: pin group (:c:type:`sk_pin_group`)
 * @values: densified value specifying which pins to toggle
 *
 * Only the pins specified in :c:type:`sk_pin_group` are affected by set operation.
 * If :c:member:`sk_pin_group.pins` are specified, for example as 0b0101000000000011,
 * and `values` is set to 0b000011110000ABCD, the resulting GPIO value will be
 * 0b0A0B0000000000CD.
 *
 * Note:
 * Access to the corresponding GPIO registers is not atomic.
 */
void sk_pin_group_toggle(sk_pin_group group, uint16_t values);


#if SK_USE_GLSK_DEFINITIONS

// some STM32F4DISCOVERY pins
extern const sk_pin sk_io_led_orange;
extern const sk_pin sk_io_led_red;
extern const sk_pin sk_io_led_green;
extern const sk_pin sk_io_led_blue;
extern const sk_pin sk_io_btn_usr;
// some GL-SK pins
extern const sk_pin sk_io_btn_right;
extern const sk_pin sk_io_btn_mid;
extern const sk_pin sk_io_btn_left;
extern const sk_pin sk_io_btn_up;
extern const sk_pin sk_io_btn_down;
extern const sk_pin sk_io_eth_led;
extern const sk_pin sk_io_lcd_bkl;
extern const sk_pin sk_io_lcd_rs;
extern const sk_pin sk_io_lcd_rw;
extern const sk_pin sk_io_lcd_en;
extern const sk_pin sk_io_spiflash_ce;

extern const sk_pin_group sk_io_lcd_data;

#if !(SK_USE_SIZE_OPTIMIZATIONS)
void glsk_pins_init(const bool set_all);
#endif
#endif
