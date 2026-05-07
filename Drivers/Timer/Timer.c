#include "Timer.h"
#include "Timer_Private.h"
#include "Bit_Operations.h"
#include "Nvic.h"

#define NUM_OF_TIMERS 4U

static uint32 Timer_BaseAddresses[NUM_OF_TIMERS] = {TIM2_BASE_ADDR, TIM3_BASE_ADDR, TIM4_BASE_ADDR, TIM5_BASE_ADDR};

/* NVIC IRQ numbers:  TIM2=28, TIM3=29, TIM4=30, TIM5=50 */
static uint8 Timer_NvicIrq[NUM_OF_TIMERS] = {28, 29, 30, 50};

static TimerCallback Timer_Callbacks[NUM_OF_TIMERS] = {0};

static TimerType *Timer_GetPeripheral(uint8 TimerId) {
    return (TimerType *) Timer_BaseAddresses[TimerId - TIMER2];
}

void Timer_Init(uint8 TimerId, uint16 Prescaler, uint16 AutoReload) {
    TimerType *timer = Timer_GetPeripheral(TimerId);

    timer->CR1 = 0; // Reset timer
    timer->PSC = Prescaler;
    timer->ARR = AutoReload;
    timer->CNT = 0;

    SET_BIT(timer->EGR, EGR_UG); // Force update to load PSC & ARR
    timer->SR = 0; // Clear the update flag that UG set
}

void Timer_Start(uint8 TimerId) {
    TimerType *tim = Timer_GetPeripheral(TimerId);
    SET_BIT(tim->CR1, CR1_CEN);
}

void Timer_Stop(uint8 TimerId) {
    TimerType *tim = Timer_GetPeripheral(TimerId);
    CLEAR_BIT(tim->CR1, CR1_CEN);
}

void Timer_DelayMs(uint8 TimerId, uint32 DelayMs) {
    TimerType *timer =(TimerType *)Timer_BaseAddresses[TimerId - TIMER2];

    timer->CR1 = 0; // Stop & reset
    timer->PSC = 15999U / 3;
    timer->ARR = (uint16) (DelayMs - 1);
    timer->CNT = 0;

    SET_BIT(timer->EGR, EGR_UG); // Load shadow registers
    timer->SR = 0; // Clear UIF caused by UG

    SET_BIT(timer->CR1, CR1_OPM); // One-pulse mode
    SET_BIT(timer->CR1, CR1_CEN); // Start counting

    while (!READ_BIT(timer->SR, SR_UIF)) {
        // Poll – CPU is blocked here
    }

    timer->SR = 0; // Clear UIF
    CLEAR_BIT(timer->CR1, CR1_CEN); // Stop counter
}


void Timer_DelayMsAsync(uint8 TimerId, uint32 DelayMs, TimerCallback Callback) {
    TimerType *timer =(TimerType *)Timer_BaseAddresses[TimerId - TIMER2];
    uint8 index = TimerId - TIMER2;
    uint8 irqNum = Timer_NvicIrq[index];

    Timer_Callbacks[index] = Callback;

    timer->CR1 = 0; // Stop & reset
    timer->PSC = 15999U / 3;
    timer->ARR = (uint16) (DelayMs - 1);
    timer->CNT = 0;

    SET_BIT(timer->EGR, EGR_UG); // Load shadow registers
    timer->SR = 0; // Clear UIF caused by UG

    SET_BIT(timer->CR1, CR1_OPM); // One-pulse mode

    SET_BIT(timer->DIER, DIER_UIE); // Enable update interrupt
    Nvic_EnableIrq(irqNum);

    SET_BIT(timer->CR1, CR1_CEN); // Start counting
}

void Timer_OcToggleInit(uint8 TimerId, uint8 Channel, uint16 Prescaler, uint16 Period) {
    TimerType *timer =(TimerType *)Timer_BaseAddresses[TimerId - TIMER2];

    /* Stop & reset */
    timer->CR1 = 0;

    /* Time-base */
    timer->PSC = Prescaler;
    timer->ARR = Period - 1;
    timer->CNT = 0;

    if (Channel <= 2) {
        uint8 shift = (Channel - 1) * 8;
        timer->CCMR1 &= ~((uint32) 0xFF << shift);
        timer->CCMR1 |= ((uint32) CCMR_OC_TOGGLE << shift);
    } else {
        uint8 shift = (Channel - 3) * 8;
        timer->CCMR2 &= ~((uint32) 0xFF << shift);
        timer->CCMR2 |= ((uint32) CCMR_OC_TOGGLE << shift);
    }

    /* Set compare value (CCRx) */
    volatile uint32 *ccr = &timer->CCR1 + (Channel - 1);
    *ccr = 0;

    /* Enable channel output (CCxE bit in CCER) */
    SET_BIT(timer->CCER, (Channel - 1) * 4);

    /* Load shadow registers & clear flags */
    SET_BIT(timer->EGR, EGR_UG);
    timer->SR = 0;

    /* Start */
    SET_BIT(timer->CR1, CR1_CEN);
}

static void Timer_HandleIrq(uint8 index) {
    TimerType *timer =(TimerType *)Timer_BaseAddresses[index];


    if (READ_BIT(timer->SR, SR_UIF)) {
        timer->SR = 0; // Clear UIF
        CLEAR_BIT(timer->DIER, DIER_UIE); // Disable further IRQs
        CLEAR_BIT(timer->CR1, CR1_CEN); // Stop counter

        if (Timer_Callbacks[index] != 0) {
            Timer_Callbacks[index]();
        }
    }
}

void TIM2_IRQHandler(void) {
    Timer_HandleIrq(0);
}
void TIM3_IRQHandler(void) {
    Timer_HandleIrq(1);
}
void TIM4_IRQHandler(void) {
    Timer_HandleIrq(2);
}
void TIM5_IRQHandler(void) {
    Timer_HandleIrq(3);
}
