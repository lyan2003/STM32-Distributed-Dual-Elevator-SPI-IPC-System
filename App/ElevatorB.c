#include "ElevatorB.h"
#include "Gpio.h"
#include "Exti.h"
#include "Pwm.h"
#include "Timer.h"
#include "Rcc.h"
#include "Nvic.h"
#include "Utils.h"

/* ========================================== */
/* Motor Speed Macros                         */
/* ========================================== */

#define SPEED_STOP   0     /* 0% Duty Cycle */
#define SPEED_SLOW   20    /* 20% Duty Cycle */
#define SPEED_FULL   100   /* 100% Duty Cycle */

/* ========================================== */
/* Global Elevator Context                    */
/* ========================================== */

static ElevatorContextB_t ElevB;

/* ========================================== */
/* EXTI Callback Functions                    */
/* ========================================== */

/**
 * @brief Emergency stop callback.
 *
 * Stops the elevator immediately and switches
 * the FSM into emergency mode.
 */
static void EmergencyB_Callback(void)
{
    ElevB.currentState = STATE_EMERGENCY_B;

    /* Hard stop */
    Pwm_SetDutyPercent(TIMER3,
                       PWM_CHANNEL_1,
                       SPEED_STOP);
}

/* ========================================== */
/* Floor Sensor Callbacks                     */
/* ========================================== */

static void Sensor1B_Callback(void) { ElevB.currentFloor = 1; }
static void Sensor2B_Callback(void) { ElevB.currentFloor = 2; }
static void Sensor3B_Callback(void) { ElevB.currentFloor = 3; }
static void Sensor4B_Callback(void) { ElevB.currentFloor = 4; }

/* ========================================== */
/* Cabin Button Callbacks                     */
/* ========================================== */

/**
 * @brief Cabin button callbacks.
 *
 * Updates the requested target floor if the
 * elevator is not in emergency mode.
 */

static void CabinBtn1B_Callback(void)
{
    if (ElevB.currentState != STATE_EMERGENCY_B) {
        ElevB.targetFloor = 1;
    }
}

static void CabinBtn2B_Callback(void)
{
    if (ElevB.currentState != STATE_EMERGENCY_B) {
        ElevB.targetFloor = 2;
    }
}

static void CabinBtn3B_Callback(void)
{
    if (ElevB.currentState != STATE_EMERGENCY_B) {
        ElevB.targetFloor = 3;
    }
}

static void CabinBtn4B_Callback(void)
{
    if (ElevB.currentState != STATE_EMERGENCY_B) {
        ElevB.targetFloor = 4;
    }
}

/* ========================================== */
/* Door Timer Callback                        */
/* ========================================== */

/**
 * @brief Executed after the door-open timeout expires.
 */
static void DoorsClosedB_Callback(void)
{
    if (ElevB.currentState == STATE_DOORS_OPEN_B) {
        ElevB.currentState = STATE_IDLE_B;
    }
}

/* ========================================== */
/* Elevator Initialization                    */
/* ========================================== */

/**
 * @brief Initialize Elevator B hardware and software modules.
 */
void ElevatorB_Init(void)
{
    /* ======================================
     * Initial Elevator State
     * ====================================== */

    ElevB.currentState = STATE_IDLE_B;
    ElevB.currentFloor = 1;
    ElevB.targetFloor  = 1;

    /* ======================================
     * Enable Peripheral Clocks
     * ====================================== */

    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);
    Rcc_Enable(RCC_SYSCFG);
    Rcc_Enable(RCC_USART1);

    /* ======================================
     * PWM Motor Initialization
     * PB4 -> Timer3 Channel1
     * ====================================== */

    Gpio_Init(GPIO_B,
              4,
              GPIO_AF,
              GPIO_PUSH_PULL);

    Gpio_ConfigAF(GPIO_B, 4, 2);

    Pwm_Init(TIMER3,
             PWM_CHANNEL_1,
             1599,
             99);

    Pwm_SetDutyPercent(TIMER3,
                       PWM_CHANNEL_1,
                       0);

    Pwm_Start(TIMER3,
              PWM_CHANNEL_1);

    /* ======================================
     * GPIO Input Initialization
     * ====================================== */

    /* Emergency button (PA0) */
    Gpio_Init(GPIO_A,
              0,
              GPIO_INPUT,
              GPIO_PULL_UP);

    /* Cabin buttons */
    Gpio_Init(GPIO_B, 1, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 2, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 3, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 5, GPIO_INPUT, GPIO_PULL_UP);

    /* Floor sensors */
    Gpio_Init(GPIO_B, 6, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 7, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 8, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 9, GPIO_INPUT, GPIO_PULL_UP);

    /* ======================================
     * EXTI Configuration
     * ====================================== */

    /* Emergency button */
    Exti_Init(EXTI_LINE_0,
              EXTI_PORT_A,
              EXTI_EDGE_FALLING,
              EmergencyB_Callback);

    /* Cabin buttons */
    Exti_Init(EXTI_LINE_1,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              CabinBtn1B_Callback);

    Exti_Init(EXTI_LINE_2,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              CabinBtn2B_Callback);

    Exti_Init(EXTI_LINE_3,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              CabinBtn3B_Callback);

    Exti_Init(EXTI_LINE_5,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              CabinBtn4B_Callback);

    /* Floor sensors */
    Exti_Init(EXTI_LINE_6,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              Sensor1B_Callback);

    Exti_Init(EXTI_LINE_7,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              Sensor2B_Callback);

    Exti_Init(EXTI_LINE_8,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              Sensor3B_Callback);

    Exti_Init(EXTI_LINE_9,
              EXTI_PORT_B,
              EXTI_EDGE_FALLING,
              Sensor4B_Callback);

    /* ======================================
     * NVIC Priority Configuration
     * (Optional)
     * ====================================== */

    /*
    Nvic_SetPriorityGrouping();

    // Emergency interrupt -> Highest priority
    Nvic_SetPriority(6, 0);

    // Cabin buttons
    Nvic_SetPriority(7, 1);
    Nvic_SetPriority(8, 1);
    Nvic_SetPriority(9, 1);

    // Sensors and EXTI9_5 group
    Nvic_SetPriority(23, 2);
    */

    /* ======================================
     * Enable EXTI Interrupts
     * ====================================== */

    for (uint8 i = 0; i <= 9; i++) {
        Exti_Enable(i);
    }
}

/* ========================================== */
/* Elevator Finite State Machine              */
/* ========================================== */

/**
 * @brief Main FSM controlling elevator behavior.
 */
void ElevatorB_RunFSM(void)
{
    switch (ElevB.currentState) {

    /* ======================================
     * Idle State
     * ====================================== */

    case STATE_IDLE_B:

        /* Stop motor */
        Pwm_SetDutyPercent(TIMER3,
                           PWM_CHANNEL_1,
                           SPEED_STOP);

        /* Determine movement direction */
        if (ElevB.targetFloor > ElevB.currentFloor) {

            ElevB.currentState = STATE_MOVING_UP_B;

        } else if (ElevB.targetFloor < ElevB.currentFloor) {

            ElevB.currentState = STATE_MOVING_DOWN_B;
        }

        break;

    /* ======================================
     * Moving States
     * ====================================== */

    case STATE_MOVING_UP_B:
    case STATE_MOVING_DOWN_B:
    {
        int dist;

        /* Calculate distance to target */
        dist = (ElevB.targetFloor > ElevB.currentFloor) ?
               (ElevB.targetFloor - ElevB.currentFloor) :
               (ElevB.currentFloor - ElevB.targetFloor);

        /* Dynamic speed control */
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
        if (ElevB.currentFloor == ElevB.targetFloor) {

            ElevB.currentState = STATE_DOORS_OPEN_B;

            /* Stop motor */
            Pwm_SetDutyPercent(TIMER3,
                               PWM_CHANNEL_1,
                               SPEED_STOP);

            /* Keep doors open for 3 seconds */
            Timer_DelayMsAsync(TIMER4,
                               3000,
                               DoorsClosedB_Callback);
        }

        break;
    }

    /* ======================================
     * Doors Open State
     * ====================================== */

    case STATE_DOORS_OPEN_B:
        break;

    /* ======================================
     * Emergency State
     * ====================================== */

    case STATE_EMERGENCY_B:

        /* Ensure motor remains stopped */
        Pwm_SetDutyPercent(TIMER3,
                           PWM_CHANNEL_1,
                           SPEED_STOP);

        break;
    }
}

/* ========================================== */
/* Getters                                    */
/* ========================================== */

/**
 * @brief Get current elevator context.
 *
 * @return Elevator B context structure.
 */
ElevatorContextB_t ElevatorB_GetContext(void)
{
    return ElevB;
}

/* ========================================== */
/* SPI Command Handler                        */
/* ========================================== */

/**
 * @brief Update target floor from SPI command.
 *
 * @param target Requested floor received via SPI.
 */
void ElevatorB_SetTargetFromSPI(uint8 target)
{
    if (ElevB.currentState != STATE_EMERGENCY_B &&
        target >= 1 &&
        target <= 4) {

        ElevB.targetFloor = target;
    }
}