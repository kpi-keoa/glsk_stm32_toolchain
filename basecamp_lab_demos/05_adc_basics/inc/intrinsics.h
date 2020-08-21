/**
 * libsk intrinsics
 *
 * We lack some intrinsic functions usually found in CMSIS. So provide our own with
 * compatible names
 */

#include "macro.h"


/** WFI - Wait For Interrupt */
inline sk_attr_alwaysinline void __WFI(void)
{
	__asm__ volatile ("wfi");
}


/** WFE - Wait For Event */
inline sk_attr_alwaysinline void __WFE(void)
{
	__asm__ volatile ("wfe");
}
