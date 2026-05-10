#include "elevator.h"
#include "Gpio.h"
#include "Exti.h"
#include "Pwm.h"
#include "Timer.h"
#include "Rcc.h"
#include "Nvic.h"
#include "Utils.h"
#include "MasterLogic.h"
#include "stm32f401xe.h"

/* ========================================== */
/* Motor Speed Macros                         */
/* ========================================== */

#define SPEED_STOP   0     /* 0% Duty Cycle */
#define SPEED_SLOW   20    /* 20% Duty Cycle */
#define SPEED_FULL   100   /* 100% Duty Cycle */

/* ========================================== */
/* Global Elevator Context                    */
/* ========================================== */

/* Volatile because values are modified inside interrupts */
static volatile ElevatorContextA_t ElevA;

/* Stores interrupted target request */
static volatile uint8 ResumeTarget = 0;

/* ========================================== */
/* Emergency Callback                         */
/* ========================================== */

/**
 * @brief Emergency interrupt callback.
 *
 * Stops the elevator immediately and switches
 * the FSM to emergency mode.
 */
static void Emergency_Callback(void)
{
    ElevA.currentState = STATE_EMERGENCY;

    /* Stop motor */
    Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0);
}

/* ========================================== */
/* Floor Sensor Callbacks                     */
/* ========================================== */

static void Sensor1_Callback(void) { ElevA.currentFloor = 1; }
static void Sensor2_Callback(void) { ElevA.currentFloor = 2; }
static void Sensor3_Callback(void) { ElevA.currentFloor = 3; }
static void Sensor4_Callback(void) { ElevA.currentFloor = 4; }

/* ========================================== */
/* Cabin Button Callbacks                     */
/* ========================================== */

static void CabinBtn1_Callback(void) { ElevatorA_SetTargetFloor(1); }
static void CabinBtn2_Callback(void) { ElevatorA_SetTargetFloor(2); }
static void CabinBtn3_Callback(void) { ElevatorA_SetTargetFloor(3); }
static void CabinBtn4_Callback(void) { ElevatorA_SetTargetFloor(4); }

/* ========================================== */
/* Door Callback                              */
/* ========================================== */

/**
 * @brief Callback executed after door open timeout.
 */
static void DoorsClosed_Callback(void)
{
    if (ElevA.currentState == STATE_DOORS_OPEN) {
        ElevA.currentState = STATE_IDLE;
    }
}

/* ========================================== */
/* Elevator Initialization                    */
/* ========================================== */

/**
 * @brief Initialize Elevator A hardware and software modules.
 */
void ElevatorA_Init(void)
{
    /* Initial elevator state */
    ElevA.currentState = STATE_IDLE;
    ElevA.currentFloor = 1;
    ElevA.targetFloor  = 1;

    /* Enable peripheral clocks */
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);
    Rcc_Enable(RCC_SYSCFG);

    /* ======================================
     * PWM Configuration
     * ====================================== */

    Gpio_Init(GPIO_B, 4, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_ConfigAF(GPIO_B, 4, 2);

    Pwm_Init(TIMER3, PWM_CHANNEL_1, 1599, 99);

    Pwm_SetDutyPercent(TIMER3, PWM_CHANNEL_1, 0);

    Pwm_Start(TIMER3, PWM_CHANNEL_1);

    /* ======================================
     * GPIO Input Initialization
     * ====================================== */

    uint8 i;

    for (i = 0; i <= 8; i++) {
        Gpio_Init(GPIO_A, i, GPIO_INPUT, GPIO_PULL_UP);
    }

    Gpio_Init(GPIO_B, 9,  GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 10, GPIO_INPUT, GPIO_PULL_UP);

    for (i = 12; i <= 15; i++) {
        Gpio_Init(GPIO_B, i, GPIO_INPUT, GPIO_PULL_UP);
    }

    /* ======================================
     * EXTI Configuration
     * ====================================== */

    /* Emergency button */
    Exti_Init(EXTI_LINE_0,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              Emergency_Callback);

    /* Cabin buttons */
    Exti_Init(EXTI_LINE_1,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              CabinBtn1_Callback);

    Exti_Init(EXTI_LINE_2,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              CabinBtn2_Callback);

    Exti_Init(EXTI_LINE_3,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              CabinBtn3_Callback);

    Exti_Init(EXTI_LINE_4,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              CabinBtn4_Callback);

    /* Floor sensors */
    Exti_Init(EXTI_LINE_5,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              Sensor1_Callback);

    Exti_Init(EXTI_LINE_6,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              Sensor2_Callback);

    Exti_Init(EXTI_LINE_7,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              Sensor3_Callback);

    Exti_Init(EXTI_LINE_8,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              Sensor4_Callback);

    /* Hall buttons handled by master logic */
    Exti_Init(EXTI_LINE_9,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              MasterLogic_F1_Up_Callback);

    Exti_Init(EXTI_LINE_10,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              MasterLogic_F2_Up_Callback);

    Exti_Init(EXTI_LINE_12,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              MasterLogic_F2_Down_Callback);

    Exti_Init(EXTI_LINE_13,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              MasterLogic_F3_Up_Callback);

    Exti_Init(EXTI_LINE_14,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              MasterLogic_F3_Down_Callback);

    Exti_Init(EXTI_LINE_15,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              MasterLogic_F4_Down_Callback);

    /* Enable all EXTI lines except line 11 */
    for (i = 0; i <= 15; i++) {

        if (i != 11) {
            Exti_Enable(i);
        }
    }
}

/* ========================================== */
/* Elevator Finite State Machine              */
/* ========================================== */

/**
 * @brief Main elevator finite state machine.
 */
void ElevatorA_RunFSM(void)
{
    switch (ElevA.currentState) {

    /* ======================================
     * Idle State
     * ====================================== */

    case STATE_IDLE:

        /* Restore previous interrupted target */
        if (ResumeTarget != 0) {

            ElevA.targetFloor = ResumeTarget;
            ResumeTarget = 0;
        }

        /* Stop motor */
        Pwm_SetDutyPercent(TIMER3,
                           PWM_CHANNEL_1,
                           SPEED_STOP);

        /* Determine movement direction */
        if (ElevA.targetFloor > ElevA.currentFloor) {

            ElevA.currentState = STATE_MOVING_UP;

        } else if (ElevA.targetFloor < ElevA.currentFloor) {

            ElevA.currentState = STATE_MOVING_DOWN;
        }

        break;

    /* ======================================
     * Moving States
     * ====================================== */

    case STATE_MOVING_UP:
    case STATE_MOVING_DOWN:
    {
        int dist;

        /* Calculate remaining distance */
        dist = (ElevA.targetFloor > ElevA.currentFloor) ?
               (ElevA.targetFloor - ElevA.currentFloor) :
               (ElevA.currentFloor - ElevA.targetFloor);

        /* Speed control based on distance */
        if (dist > 1) {

            Pwm_SetDutyPercent(TIMER3,
                               PWM_CHANNEL_1,
                               SPEED_FULL);

        } else {

            Pwm_SetDutyPercent(TIMER3,
                               PWM_CHANNEL_1,
                               SPEED_SLOW);
        }

        /* Target reached */
        if (ElevA.currentFloor == ElevA.targetFloor) {

            ElevA.currentState = STATE_DOORS_OPEN;

            /* Stop motor */
            Pwm_SetDutyPercent(TIMER3,
                               PWM_CHANNEL_1,
                               SPEED_STOP);

            /* Keep doors open for 3 seconds */
            Timer_DelayMsAsync(TIMER4,
                               3000,
                               DoorsClosed_Callback);
        }

        break;
    }

    /* ======================================
     * Doors Open State
     * ====================================== */

    case STATE_DOORS_OPEN:
        break;

    /* ======================================
     * Emergency State
     * ====================================== */

    case STATE_EMERGENCY:

        /* Ensure motor remains stopped */
        Pwm_SetDutyPercent(TIMER3,
                           PWM_CHANNEL_1,
                           SPEED_STOP);

        break;
    }
}

/* ========================================== */
/* Context Access                             */
/* ========================================== */

/**
 * @brief Get current elevator context.
 *
 * @return Elevator context structure.
 */
ElevatorContextA_t ElevatorA_GetContext(void)
{
    return ElevA;
}

/* ========================================== */
/* Target Floor Management                    */
/* ========================================== */

/**
 * @brief Set a new elevator target floor.
 *
 * Includes protection against race conditions
 * caused by interrupts.
 *
 * @param target Requested floor.
 */
void ElevatorA_SetTargetFloor(uint8 target)
{
    __disable_irq();

    if (ElevA.currentState != STATE_EMERGENCY) {

        /* Open doors immediately if already at target */
        if (ElevA.currentState == STATE_IDLE &&
            ElevA.currentFloor == target) {

            ElevA.currentState = STATE_DOORS_OPEN;

            Timer_DelayMsAsync(TIMER4,
                               3000,
                               DoorsClosed_Callback);
        }
        else {

            /*
             * Save interrupted target while handling
             * a higher-priority intermediate request.
             */

            if (ElevA.currentState == STATE_MOVING_UP &&
                target > ElevA.currentFloor &&
                target < ElevA.targetFloor) {

                ResumeTarget = ElevA.targetFloor;
            }
            else if (ElevA.currentState == STATE_MOVING_DOWN &&
                     target < ElevA.currentFloor &&
                     target > ElevA.targetFloor) {

                ResumeTarget = ElevA.targetFloor;
            }

            /* Move to the new target first */
            ElevA.targetFloor = target;
        }
    }

    __enable_irq();
}