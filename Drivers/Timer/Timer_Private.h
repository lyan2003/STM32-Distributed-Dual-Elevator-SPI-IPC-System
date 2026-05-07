#ifndef TIMER_PRIVATE_H
#define TIMER_PRIVATE_H

#include "Std_Types.h"

typedef struct {
    volatile uint32 CR1;
    volatile uint32 CR2;
    volatile uint32 SMCR;
    volatile uint32 DIER;
    volatile uint32 SR;
    volatile uint32 EGR;
    volatile uint32 CCMR1;
    volatile uint32 CCMR2;
    volatile uint32 CCER;
    volatile uint32 CNT;
    volatile uint32 PSC;
    volatile uint32 ARR;
    volatile uint32 _reserved1;
    volatile uint32 CCR1;
    volatile uint32 CCR2;
    volatile uint32 CCR3;
    volatile uint32 CCR4;
    volatile uint32 _reserved2;
    volatile uint32 DCR;
    volatile uint32 DMAR;
    volatile uint32 OR;
} TimerType;

#define TIM2_BASE_ADDR   0x40000000UL
#define TIM3_BASE_ADDR   0x40000400UL
#define TIM4_BASE_ADDR   0x40000800UL
#define TIM5_BASE_ADDR   0x40000C00UL

#define CR1_CEN          0U
#define CR1_URS          2U
#define CR1_OPM          3U
#define CR1_ARPE         7U

#define DIER_UIE         0U

#define SR_UIF           0U

#define EGR_UG           0U

#define CCMR_OC_PWM1_PRELOAD  0x68U

#define CCMR_OC_TOGGLE        0x30U

#endif