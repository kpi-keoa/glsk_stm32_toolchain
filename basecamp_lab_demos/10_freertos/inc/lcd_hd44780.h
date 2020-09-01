/**
 * libsk GL-SK on-board LCD abstraction layer
 *
 * The display is WH1602B (based on HD44780 controller)
 */

#include "errors.h"
#include "pin.h"
#include <stdint.h>


/** Pointer to delay(uint32_t var) function defined as type */
typedef void (*sk_delay_func_t)(uint32_t);


struct sk_lcd {
	/** HD44780 data pins (DB0..DB7 or DB4..DB7) represented as :c:type:`sk_pin_group` */
	sk_pin_group *pin_group_data;
	/** HD44780 register select pin (RS) represented as :c:type:`sk_pin` */
	sk_pin *pin_rs;
	/** HD44780 enable pin (E) represented as :c:type:`sk_pin` */
	sk_pin *pin_en;
	/** HD44780 enable pin (R/W) represented as :c:type:`sk_pin`.
     *  Set to NULL if not used (always grounded on board) */
	sk_pin *pin_rw;
	/** Display backlight pin. Set to NULL if not used */
	sk_pin *pin_bkl;
	/** Pointer to backlight control function (i.e. to set backlight LED PWM level).
      * Set to NULL if not used. This way only two levels will be possible
      * (0 for OFF and != 0 for ON) */
	void (*set_backlight_func)(uint8_t);
	/** Pointer to user-provided delay function with microsecond resolution.
	  * Set to NULL to use ms delay as a fallback */
	sk_delay_func_t delay_func_us;
	/** Pointer to user-provided delay function with millisecond resolution.
      * Set to NULL to use us delay as a fallback */
	sk_delay_func_t delay_func_ms;
	/** True for 4-bit HD44780 interface, False for 8-bit. Only 4-bit IF is supported for now */
	unsigned int is4bitinterface : 1;

	/** Function which maps characters to LCD symbol table values.
      * Set to NULL to use :c:func:`lcd_charmap_none` as default */
	uint8_t (*charmap_func)(const char);
	// private (mangled) members
	/** Private: internally set to True after initialization was issued */
	unsigned int __isinitialized : 1;
};


/**
 * Issue low-level LCD command
 * @rs: value on RS pin
 * @rw: write (`true`) or read (`false`). Corresponds to value on RW pin
 * @cmddata: value to set on data pins
 */
sk_err _sk_lcd_cmd(struct sk_lcd *lcd, bool rs, bool rw, uint8_t *cmddata);


/**
 * Clear Display command
 * @lcd: LCD object (:c:type:`sk_lcd`)
 *
 * Clears entire display and sets DDRAM address 0 in address counter.
 */
sk_err sk_lcd_cmd_clear(struct sk_lcd *lcd);


/**
 * Return Home command
 * @lcd: LCD object (:c:type:`sk_lcd`)
 *
 * Sets DDRAM address 0 in address counter. Also returns display from being shifted to original
 * position. DDRAM contents remain unchanged.
 */
sk_err sk_lcd_cmd_rethome(struct sk_lcd *lcd);


/**
 * Entry Mode Set command
 * @lcd: LCD object (:c:type:`sk_lcd`)
 * @isdirright: `true` for left-to-right direction (cursor increment),
 *              `false` for right-to-left direction (cursor decrement)
 * @isshift: whether to shift display contents according to direction set
 *
 * Sets cursor move direction and specifies display shift.
 * These operations are performed during data write and read
 */
sk_err sk_lcd_cmd_emodeset(struct sk_lcd *lcd, bool isdirright, bool isshift);


/**
 * Display ON/OFF Control command
 * @lcd: 	 LCD object (:c:type:`sk_lcd`)
 * @display: turn display on (`true`) or off (`false`)
 * @cursor:  turn cursor dispaly on (`true`) or off (`false`)
 * @blink:	 turn cursor blinking on (`true`) or off (`false`)
 *
 * Sets entire display (D) on/off, cursor on/off (C), and blinking of cursor position character (B)
 */
sk_err sk_lcd_cmd_onoffctl(struct sk_lcd *lcd, bool display, bool cursor, bool blink);


/**
 * Cursor or Display Shift command
 * @lcd: 	    LCD object (:c:type:`sk_lcd`)
 * @isshift:    Display shift (`true`) or Cursor move (`false`)
 * @isdirright: Shift/Move to the right (`true`) or to the left (`false`)
 * 
 * Moves cursor and shifts display without changing DDRAM contents
 */
sk_err sk_lcd_cmd_shift(struct sk_lcd *lcd, bool isshift, bool isdirright);

/**
 * Set DDRAM Address and Set CGRAM Address commands
 * @lcd:LCD object (:c:type:`sk_lcd`)
 * @addr: address to set
 * @iscgram: `false` to set provided address in DDRAM, `true` -- in CGRAM
 *
 * Sets DDRAM/CGRAM address. DDRAM/CGRAM data is sent and received after this setting
 * DDRAM address has 7 bits, CGRAM address is 6 bit wide
 */
sk_err sk_lcd_cmd_setaddr(struct sk_lcd *lcd, uint8_t addr, bool iscgram);


/**
 * Write data to CG or DDRAM
 * @lcd: LCD object (:c:type:`sk_lcd`)
 * @byte: data byte to send
 */
void sk_lcd_write_byte(struct sk_lcd *lcd, uint8_t byte);

/**
 * Put character at current position on LCD
 * @lcd: LCD object (:c:type:`sk_lcd`)
 * @ch: char to be mapped using charmap_func and sent to LCD
 */
void sk_lcd_putchar(struct sk_lcd *lcd, const char ch);


/**
 * Set backlight level (0..255)
 * @lcd:   LCD object (:c:type:`sk_lcd`)
 * @level: Backlight level
 *
 * Note:
 * Only two levels (ON/OFF state) are used when no backlight control callback function
 * was provided by the user. In this case, 0 means OFF and != 0 means ON.
 */
sk_err sk_lcd_set_backlight(struct sk_lcd *lcd, uint8_t level);


/**
 * Performs initalization of LCD using pre-set configuration from LCD object structure
 * @lcd:   LCD object (:c:type:`sk_lcd`)
 */
sk_err sk_lcd_init(struct sk_lcd *lcd);


// Character map functions
// Charmap function maps characters to LCD symbol table values

/** Don't map. Use direct symbol values (1:1) */
uint8_t sk_lcd_charmap_none(const char c);


/** CP1251 (aka Windows-1251) char map
  *
  * This requires to compile with
  *   -finput-charset=UTF-8    as our sources are encoded in UTF-8
  *   -fexec-charset=cp1251    as we expect each char to be encoded in CP1251 when executing
  */
uint8_t sk_lcd_charmap_rus_cp1251(const char c);
