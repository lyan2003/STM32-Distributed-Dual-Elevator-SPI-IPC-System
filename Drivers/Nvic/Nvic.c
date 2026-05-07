#include "Nvic.h"

typedef struct {
    volatile uint32 NVIC_ISER[8];
    uint32 _r[24];
    volatile uint32 NVIC_ICER[8];
} NvicType;

#define NVIC          ((NvicType*)0xE000E100)


void Nvic_EnableIrq(uint8 IrqNumber) {
    NVIC->NVIC_ISER[IrqNumber / 32] |= (0x01 << (IrqNumber % 32));
}


void Nvic_DisableIrq(uint8 IrqNumber) {
    NVIC->NVIC_ICER[IrqNumber / 32] |= (0x01 << (IrqNumber % 32));
}

void Nvic_SetPriority(uint8 IrqNumber, uint8 Priority) {
    /* The NVIC Priority Registers start at 0xE000E400 */
    volatile uint8 *NVIC_IPR = (volatile uint8 *)0xE000E400;
    NVIC_IPR[IrqNumber] = (Priority << 4);
}

void Nvic_SetPriorityGrouping(void) {
    /* SCB AIRCR Register Address */
    volatile uint32 *SCB_AIRCR = (volatile uint32 *)0xE000ED0C;
    *SCB_AIRCR = 0x05FA0000 | (3 << 8);
}