#ifndef ELEVATOR_H
#define ELEVATOR_H

#include "Std_Types.h"

/* The mandated FSM States */
typedef enum {
    STATE_IDLE = 0,
    STATE_MOVING_UP,
    STATE_MOVING_DOWN,
    STATE_DOORS_OPEN,
    STATE_EMERGENCY
} ElevatorState_t;

/* The Context Struct: Holds the current reality of Elevator A */
typedef struct {
    volatile ElevatorState_t currentState;
    volatile uint8 currentFloor;
    volatile uint8 targetFloor;
} ElevatorContext_t;

/* Public API */
void ElevatorA_Init(void);
void ElevatorA_RunFSM(void);

/* Getter so SPI can read the state*/
ElevatorContext_t ElevatorA_GetContext(void);

#endif /* ELEVATOR_H */