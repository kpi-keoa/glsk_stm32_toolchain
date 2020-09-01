/**
 * libsk intrinsics
 *
 * We lack some intrinsic functions usually found in CMSIS. So provide our own with
 * compatible names
 */

#include "macro.h"
#include <stdint.h>


/** WFI - Wait For Interrupt */
inline sk_attr_alwaysinline void __WFI(void)
{
	__asm__ volatile ("wfi" ::: "memory");
}


/** WFE - Wait For Event */
inline sk_attr_alwaysinline void __WFE(void)
{
	__asm__ volatile ("wfe" ::: "memory");
}


/** DMB - Data Memory Barrier
 *
 * The DMB instruction that all explicit data memory transfers before the DMB are completed
 * before any subsequent data memory transfers after the DMB starts
 */
inline sk_attr_alwaysinline void __DMB(void)
{
	__asm__ volatile ("dmb" ::: "memory");
}


/** DSB - Data Synchronization Barrier
 *
 * The DSB instruction ensures all explicit data transfers before the DSB are complete before any
 * instruction after the DSB is executed
 */
inline sk_attr_alwaysinline void __DSB(void)
{
	__asm__ volatile ("dsb" ::: "memory");
}


/** LDREXB - LDR Exclusive (8 bit)
 *  @addr: Pointer to 8 bit data
 *  @return: Value pointed by `ptr`
 *
 * Executes a exclusive LDR instruction for 8 bit value
 */
inline sk_attr_alwaysinline uint8_t __LDREXB(volatile uint8_t *addr)
{
	// Exclusively load to 32-bit register Rt from memory with address stored in Rn
	// For more see:
	// https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
	// https://gcc.gnu.org/onlinedocs/gcc/Constraints.html
	// https://gcc.gnu.org/onlinedocs/gcc/Modifiers.html
	// and p. 79 Programming Manual:   3.4.8. LDREX and STREX
	// "=r" - means %.. is replaced with a general register operand where value is written
	// "Q"  - A memory reference where the exact address is in a single register 
	//        (‘‘m’’ is preferable for asm statements)
	uint32_t result;
	__asm__ volatile ("ldrexb %0, %1" : "=r" (result) : "Q" (*addr) );
	return ((uint8_t) result);
}


/** STREXB - STR Exclusive (8 bit)
 *  @value: Value to store in address pointed by `addr`
 *  @addr: Pointer to 8 bit data
 *  @return: `0` if exclusive store succeded, `1` if failed
 *
 * Executes a exclusive STR instruction for 8 bit value
 */
inline sk_attr_alwaysinline uint32_t __STREXB(uint8_t value, volatile uint8_t *addr)
{
   uint32_t result;
   __asm__ volatile ("strexb %0, %2, %1" : "=&r" (result), "=Q" (*addr) : "r" ((uint32_t)value) );
   return(result);
}


/** CLREX - Clear Exclusive
 *
 * CLREX makes the next STREX instruction write 1 to its destination and fail to perform the store.
 * It is useful in exception handler code to force the failure of the store exclusive if the
 * exception occurs between a load exclusive instruction and the matching store exclusive 
 * instruction in a synchronization operation.
 */
inline sk_attr_alwaysinline void __CLREX(void)
{
  __asm__ volatile ("clrex" ::: "memory");
}
