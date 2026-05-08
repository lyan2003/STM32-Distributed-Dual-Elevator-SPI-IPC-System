#include "ElevatorB.h"
#include "Gpio.h"
#include "Exti.h"
#include "Pwm.h"
#include "Timer.h"
#include "Rcc.h"
#include "Nvic.h"
#include "Utils.h"

// ==========================================
// (Motor Speed Macros)
// ==========================================
#define SPEED_STOP   0    // 0% Duty Cycle (LED Off)
#define SPEED_SLOW   20   // 20% Duty Cycle (LED Dim)
#define SPEED_FULL   100  // 100% Duty Cycle (LED Fully ON)

// ==========================================
// Forward Declarations
// ==========================================
static ElevatorContextB_t ElevB;

// ---------------------------------------------------------
// EXTI CALLBACKS (Interrupt Handlers)
// ---------------------------------------------------------

/* Emergency Stop (Highest Priority - PA0) */
static void EmergencyB_Callback(void) {
    ElevB.currentState = STATE_EMERGENCY_B;
    Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, SPEED_STOP); // Hard stop
}

/* Floor Sensor Callbacks (PB6, PB7, PB8, PB9) */
static void Sensor1B_Callback(void) { ElevB.currentFloor = 1; }
static void Sensor2B_Callback(void) { ElevB.currentFloor = 2; }
static void Sensor3B_Callback(void) { ElevB.currentFloor = 3; }
static void Sensor4B_Callback(void) { ElevB.currentFloor = 4; }

/* Cabin Button Callbacks (PB1, PB2, PB3, PB5) -> PB4 is PWM */
static void CabinBtn1B_Callback(void) { if(ElevB.currentState != STATE_EMERGENCY_B) ElevB.targetFloor = 1; }
static void CabinBtn2B_Callback(void) { if(ElevB.currentState != STATE_EMERGENCY_B) ElevB.targetFloor = 2; }
static void CabinBtn3B_Callback(void) { if(ElevB.currentState != STATE_EMERGENCY_B) ElevB.targetFloor = 3; }
static void CabinBtn4B_Callback(void) { if(ElevB.currentState != STATE_EMERGENCY_B) ElevB.targetFloor = 4; }

/* Non-Blocking Timer Callback for Doors */
static void DoorsClosedB_Callback(void) {
    if (ElevB.currentState == STATE_DOORS_OPEN_B) {
        ElevB.currentState = STATE_IDLE_B;
    }
}

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------

void ElevatorB_Init(void) {
    /* 1. Initial State */
    ElevB.currentState = STATE_IDLE_B;
    ElevB.currentFloor = 1;
    ElevB.targetFloor = 1;

    /* 2. Enable Peripheral Clocks */
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);
    Rcc_Enable(RCC_SYSCFG);
    Rcc_Enable(RCC_USART1);

    /* 3. Initialize Motor (PWM on PB4 - Timer 3, Ch 1) */
    Gpio_Init(GPIO_B, 4, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_ConfigAF(GPIO_B, 4, 2);
    Pwm_Init(TIMER3, PWM_CHANNEL_1, 1599, 99);
    Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0);
    Pwm_Start(TIMER3, PWM_CHANNEL_1);

    /* 4. Initialize Inputs for Board B */
    // Emergency (PA0)
    Gpio_Init(GPIO_A, 0, GPIO_INPUT, GPIO_PULL_UP);

    // Cabin Buttons (PB1, PB2, PB3, PB5)
    Gpio_Init(GPIO_B, 1, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 2, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 3, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 5, GPIO_INPUT, GPIO_PULL_UP);

    // Sensors (PB6, PB7, PB8, PB9)
    Gpio_Init(GPIO_B, 6, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 7, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 8, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 9, GPIO_INPUT, GPIO_PULL_UP);

    /* 5. Attach EXTI Callbacks */
    Exti_Init(EXTI_LINE_0, EXTI_PORT_A, EXTI_EDGE_FALLING, EmergencyB_Callback);

    Exti_Init(EXTI_LINE_1, EXTI_PORT_B, EXTI_EDGE_FALLING, CabinBtn1B_Callback);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_B, EXTI_EDGE_FALLING, CabinBtn2B_Callback);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_B, EXTI_EDGE_FALLING, CabinBtn3B_Callback);
    Exti_Init(EXTI_LINE_5, EXTI_PORT_B, EXTI_EDGE_FALLING, CabinBtn4B_Callback);

    Exti_Init(EXTI_LINE_6, EXTI_PORT_B, EXTI_EDGE_FALLING, Sensor1B_Callback);
    Exti_Init(EXTI_LINE_7, EXTI_PORT_B, EXTI_EDGE_FALLING, Sensor2B_Callback);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_B, EXTI_EDGE_FALLING, Sensor3B_Callback);
    Exti_Init(EXTI_LINE_9, EXTI_PORT_B, EXTI_EDGE_FALLING, Sensor4B_Callback);

    // /* 6. Set NVIC Priorities */
    // Nvic_SetPriorityGrouping();
    //
    // // Emergency (Highest Priority - 0)
    // Nvic_SetPriority(6, 0);  // EXTI0
    //
    // // Cabin Buttons (Priority 1)
    // Nvic_SetPriority(7, 1);  // EXTI1
    // Nvic_SetPriority(8, 1);  // EXTI2
    // Nvic_SetPriority(9, 1);  // EXTI3
    //
    // // Sensors and Button 4 (Priority 2 - Lowest)
    // // (EXTI9_5 includes lines 5, 6, 7, 8, 9)
    // Nvic_SetPriority(23, 2);

    /* 7. Enable Interrupts */
    for(uint8 i = 0; i <= 9; i++) Exti_Enable(i);
}

// ---------------------------------------------------------
// FSM (State Machine)
// ---------------------------------------------------------
void ElevatorB_RunFSM(void) {
    switch (ElevB.currentState) {

        case STATE_IDLE_B:
            Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, SPEED_STOP);

            if (ElevB.targetFloor > ElevB.currentFloor) {
                ElevB.currentState = STATE_MOVING_UP_B;
            } else if (ElevB.targetFloor < ElevB.currentFloor) {
                ElevB.currentState = STATE_MOVING_DOWN_B;
            }
            break;


        case STATE_MOVING_UP_B:
        case STATE_MOVING_DOWN_B:
            Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, SPEED_FULL);

            if (ElevB.currentFloor == ElevB.targetFloor) {
                ElevB.currentState = STATE_DOORS_OPEN_B;
                Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, SPEED_STOP);

                Timer_DelayMsAsync(TIMER4, 3000, DoorsClosedB_Callback);
            }
            break;

        case STATE_DOORS_OPEN_B:
            break;

        case STATE_EMERGENCY_B:
            Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, SPEED_STOP);
            break;
    }
}

// ---------------------------------------------------------
// Getters & Setters
// ---------------------------------------------------------
ElevatorContextB_t ElevatorB_GetContext(void) {
    return ElevB;
}

// ==========================================
// SPI Commands Handler
// ==========================================
void ElevatorB_SetTargetFromSPI(uint8 target) {
    if (ElevB.currentState != STATE_EMERGENCY_B && target >= 1 && target <= 4) {
        ElevB.targetFloor = target;
    }
}