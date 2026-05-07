#include "elevator.h"
#include "Gpio.h"
#include "Exti.h"
#include "Pwm.h"
#include "Timer.h"
#include "Rcc.h"
#include "Nvic.h"
#include "Utils.h"

static ElevatorContext_t ElevA;

// EXTI CALLBACKS (Interrupt Handlers)


/* Emergency Stop (Highest Priority) */
static void Emergency_Callback(void) {
    ElevA.currentState = STATE_EMERGENCY;
    Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0);
}

/* Floor Sensor Callbacks (Position Tracking) */
static void Sensor1_Callback(void) { ElevA.currentFloor = 1; }
static void Sensor2_Callback(void) { ElevA.currentFloor = 2; }
static void Sensor3_Callback(void) { ElevA.currentFloor = 3; }
static void Sensor4_Callback(void) { ElevA.currentFloor = 4; }

/* Cabin Button Callbacks (Internal Requests) */

//
// /* Testing priority*/
// static void CabinBtn1_Callback(void) {
//     if(ElevA.currentState != STATE_EMERGENCY) {
//         ElevA.targetFloor = 1;
//
//         // Turn on the motor to 20% to visually indicate we are trapped
//         Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 100);
//
//         // Block the CPU for 5 entire seconds!
//         delay_ms(5000);
//
//         // Turn it off when the trap is over
//         Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0);
//     }
// }

static void CabinBtn1_Callback(void) { if(ElevA.currentState != STATE_EMERGENCY) ElevA.targetFloor = 1; }
static void CabinBtn2_Callback(void) { if(ElevA.currentState != STATE_EMERGENCY) ElevA.targetFloor = 2; }
static void CabinBtn3_Callback(void) { if(ElevA.currentState != STATE_EMERGENCY) ElevA.targetFloor = 3; }
static void CabinBtn4_Callback(void) { if(ElevA.currentState != STATE_EMERGENCY) ElevA.targetFloor = 4; }

/* Non-Blocking Timer Callback for Doors */
static void DoorsClosed_Callback(void) {
    if (ElevA.currentState == STATE_DOORS_OPEN) {
        ElevA.currentState = STATE_IDLE; // Doors closed, ready for next call
    }
}

// INITIALIZATION

void ElevatorA_Init(void) {
    /* 1. Initial State */
    ElevA.currentState = STATE_IDLE;
    ElevA.currentFloor = 1;
    ElevA.targetFloor = 1;

    /* Enable Peripheral Clocks */
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_TIM3); // For PWM
    Rcc_Enable(RCC_TIM4); // For Non-blocking Delays
    Rcc_Enable(RCC_SYSCFG);

    /* Initialize Motor (PWM on PB4 - Timer 3, Ch 1) */
    Gpio_Init(GPIO_B, 4, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_ConfigAF(GPIO_B, 4, 2); // AF2 connects PB4 to TIM3
    Pwm_Init(TIMER3, PWM_CHANNEL_1, 1599, 99); // 10kHz frequency
    Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0); // Start stopped
    Pwm_Start(TIMER3, PWM_CHANNEL_1);

    /* Initialize Inputs */
    uint8 i;
    // PA0 to PA8
    for(i = 0; i <= 8; i++) Gpio_Init(GPIO_A, i, GPIO_INPUT, GPIO_PULL_UP);
    // PB9, PB10, PB12 to PB15 (Hallway Calls)
    Gpio_Init(GPIO_B, 9, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 10, GPIO_INPUT, GPIO_PULL_UP);
    for(i = 12; i <= 15; i++) Gpio_Init(GPIO_B, i, GPIO_INPUT, GPIO_PULL_UP);

    /* Attach EXTI Callbacks */
    Exti_Init(EXTI_LINE_0, EXTI_PORT_A, EXTI_EDGE_FALLING, Emergency_Callback);

    Exti_Init(EXTI_LINE_1, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn1_Callback);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn2_Callback);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn3_Callback);
    Exti_Init(EXTI_LINE_4, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn4_Callback);
    Exti_Init(EXTI_LINE_5, EXTI_PORT_A, EXTI_EDGE_FALLING, Sensor1_Callback);
    Exti_Init(EXTI_LINE_6, EXTI_PORT_A, EXTI_EDGE_FALLING, Sensor2_Callback);
    Exti_Init(EXTI_LINE_7, EXTI_PORT_A, EXTI_EDGE_FALLING, Sensor3_Callback);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_A, EXTI_EDGE_FALLING, Sensor4_Callback);
    


    Exti_Init(EXTI_LINE_9, EXTI_PORT_B, EXTI_EDGE_FALLING, 0);
    Exti_Init(EXTI_LINE_10, EXTI_PORT_B, EXTI_EDGE_FALLING, 0);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_B, EXTI_EDGE_FALLING, 0);
    Exti_Init(EXTI_LINE_13, EXTI_PORT_B, EXTI_EDGE_FALLING, 0);
    Exti_Init(EXTI_LINE_14, EXTI_PORT_B, EXTI_EDGE_FALLING, 0);
    Exti_Init(EXTI_LINE_15, EXTI_PORT_B, EXTI_EDGE_FALLING, 0);

    // /*Set NVIC Priorities to fulfill rubric requirements */
    // Nvic_SetPriorityGrouping();
    // // EXTI 0 through 4 (Cabin Buttons & Sensor 1) - Priority 1 (Lower)
    // Nvic_SetPriority(6, 0);  // EXTI0
    // Nvic_SetPriority(7, 1);  // EXTI1
    // Nvic_SetPriority(8, 1);  // EXTI2
    // Nvic_SetPriority(9, 1);  // EXTI3
    // Nvic_SetPriority(10, 1); // EXTI4
    //
    // // EXTI 9 through 15 (Hallway Buttons) - Priority 1 (Lower)
    // Nvic_SetPriority(40, 1); // EXTI15_10
    //
    // // EXTI 5 through 9 (Sensors 2,3,4 AND EMERGENCY) is IRQ 23.
    // Nvic_SetPriority(23, 1);

    /* Enable Interrupts */
    for(i = 0; i <= 8; i++) Exti_Enable(i);
}

// FSM
void ElevatorA_RunFSM(void) {
    switch (ElevA.currentState) {
        
        case STATE_IDLE:
            Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0);
            
            if (ElevA.targetFloor > ElevA.currentFloor) {
                ElevA.currentState = STATE_MOVING_UP;
            } else if (ElevA.targetFloor < ElevA.currentFloor) {
                ElevA.currentState = STATE_MOVING_DOWN;
            }
            break;

        case STATE_MOVING_UP:
        case STATE_MOVING_DOWN:
            Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 100); // Motor running
            
            // Check if we arrived
            if (ElevA.currentFloor == ElevA.targetFloor) {
                ElevA.currentState = STATE_DOORS_OPEN;
                Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0); // Stop motor
                
                // Trigger 3-second non-blocking delay to close doors
                Timer_DelayMsAsync(TIMER4, 3000, DoorsClosed_Callback);
            }
            break;

        case STATE_DOORS_OPEN:
            // Do nothing here. The Timer4 interrupt will push us back to IDLE.
            break;

        case STATE_EMERGENCY:
            Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0); // HARD STOP
            break;
    }
}

ElevatorContext_t ElevatorA_GetContext(void) {
    return ElevA;
}