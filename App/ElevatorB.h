#ifndef ELEVATOR_B_H
#define ELEVATOR_B_H

#include "Std_Types.h"

/* The mandated FSM States (Shared structure, independent context) */
typedef enum {
    STATE_IDLE_B = 0,
    STATE_MOVING_UP_B,
    STATE_MOVING_DOWN_B,
    STATE_DOORS_OPEN_B,
    STATE_EMERGENCY_B
} ElevatorStateB_t;

/* Context Struct for Board B */
typedef struct {
    volatile ElevatorStateB_t  currentState;
    volatile uint8            currentFloor;
    volatile uint8            targetFloor;
} ElevatorContextB_t;

/* Public API */
void ElevatorB_Init(void);
void ElevatorB_RunFSM(void);

/* Getter and Setter for SPI Communication */
ElevatorContextB_t ElevatorB_GetContext(void);

void ElevatorB_SetTargetFromSPI(uint8 target);

#endif /* ELEVATOR_B_H */