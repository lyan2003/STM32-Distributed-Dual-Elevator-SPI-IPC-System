#include "MasterLogic.h"
#include "elevator.h"
#include "Ipc.h"
#include <stdlib.h>
#include "stm32f401xe.h"
#include "Dma.h"
#include <string.h>

/* ========================================== */
/* External IPC Packet                        */
/* ========================================== */

/*
 * Packet transmitted from master to slave.
 * Defined externally in main.c
 */
extern IpcPacket_t MasterTxPacket;

/* ========================================== */
/* Pending Calls Queue                        */
/* ========================================== */

/*
 * Stores pending hall calls that could not
 * be immediately assigned.
 *
 * Index = floor number
 * Value = direction
 * 1  -> Up
 * -1  -> Down
 * 0  -> No pending request
 */
static int PendingCalls[5] = {0};

/* DMA Logging Helper  */
static void MasterLog_DMA(const char* msg)
{
    /* Only transmit if the DMA stream is idle to prevent collisions */
    if (Dma_IsTransferComplete(DMA_ID_2, 7)) {
        Dma_StartTransfer(DMA_ID_2, 7, (uint32)msg, strlen(msg));
        USART1->DR = '\r';
    }
}

/* ========================================== */
/* Initialization                             */
/* ========================================== */

/**
 * @brief Initialize master dispatch logic.
 */
void MasterLogic_Init(void)
{
    int i;

    for (i = 0; i < 5; i++) {
        PendingCalls[i] = 0;
    }
}

/* ========================================== */
/* Helper Functions                           */
/* ========================================== */

/**
 * @brief Check if elevator movement perfectly matches
 *        the hall request direction.
 *
 * A perfect match occurs when:
 * - Elevator is already moving toward the request
 * - Elevator direction matches request direction
 *
 * @param state         Elevator state.
 * @param current_floor Elevator current floor.
 * @param call_floor    Requested floor.
 * @param call_dir      Requested direction.
 *
 * @return 1 if perfect match exists, otherwise 0.
 */
uint8 Is_Perfect_Match(uint8 state,
                       uint8 current_floor,
                       uint8 call_floor,
                       int call_dir)
{
    /* Moving up and request is above */
    if (state == 1 &&
        call_dir == 1 &&
        current_floor < call_floor) {

        return 1;
    }

    /* Moving down and request is below */
    if (state == 2 &&
        call_dir == -1 &&
        current_floor > call_floor) {

        return 1;
    }

    return 0;
}

/* ========================================== */
/* Main Dispatch Logic                        */
/* ========================================== */

/**
 * @brief Assign a hall request to the best elevator.
 *
 * @param call_floor Requested floor.
 * @param call_dir   Requested direction.
 */
void MasterLogic_DispatchCall(uint8 call_floor,
                              sint8 call_dir)
{
    __disable_irq();

    /* ======================================
     * Retrieve elevator states
     * ====================================== */

    ElevatorContextA_t ctxA = ElevatorA_GetContext();

    IpcPacket_t rxPktB = Ipc_MasterGetRxPacket();

    uint8 isLinkOk = Ipc_MasterLinkOk();

    /* ======================================
     * CASE 1: Communication Fault
     * ====================================== */

    if (!isLinkOk) {

        MasterLog_DMA(
            ">>> EVENT: [CASE 1] COMM FAULT! "
            "Master acting alone.\r\n"
        );

        ElevatorA_SetTargetFloor(call_floor);

        __enable_irq();
        return;
    }

    /* ======================================
     * CASE 2: Immediate Match
     * Elevator already at requested floor
     * ====================================== */

    if (ctxA.currentState == 0 &&
        ctxA.currentFloor == call_floor) {

        MasterLog_DMA(
            ">>> EVENT: [CASE 2] IMMEDIATE MATCH "
            "(A is already here)\r\n"
        );

        ElevatorA_SetTargetFloor(call_floor);

        __enable_irq();
        return;
    }

    if (rxPktB.State == 0 &&
        rxPktB.CurrentFloor == call_floor) {

        MasterLog_DMA(
            ">>> EVENT: [CASE 2] IMMEDIATE MATCH "
            "(B is already here)\r\n"
        );

        MasterTxPacket.TargetFloor = call_floor;

        __enable_irq();
        return;
    }

    /* ======================================
         * CASE 3: Perfect Match
         * Elevator moving in same direction
         * ====================================== */

    uint8 matchA =
        Is_Perfect_Match(ctxA.currentState,
                         ctxA.currentFloor,
                         call_floor,
                         call_dir);

    uint8 matchB =
        Is_Perfect_Match(rxPktB.State,
                         rxPktB.CurrentFloor,
                         call_floor,
                         call_dir);

    /* Only Elevator A matches */
    if (matchA && !matchB) {

        MasterLog_DMA(
            ">>> EVENT: [CASE 3] PERFECT MATCH "
            "(A taking call on its way)\r\n"
        );

        ElevatorA_SetTargetFloor(call_floor);

        __enable_irq();
        return;
    }

    /* Only Elevator B matches */
    else if (!matchA && matchB) {

        MasterLog_DMA(
            ">>> EVENT: [CASE 3] PERFECT MATCH "
            "(B taking call on its way)\r\n"
        );

        MasterTxPacket.TargetFloor = call_floor;

        __enable_irq();
        return;
    }

    /* Both elevators match */
    else if (matchA && matchB) {

        /* Select nearest elevator and print as ONE single string! */
        if (abs((int)ctxA.currentFloor - call_floor) <=
            abs((int)rxPktB.CurrentFloor - call_floor)) {

            MasterLog_DMA(
                ">>> EVENT: [CASE 3] PERFECT MATCH "
                "(Both Match! A is closer & dispatched)\r\n"
            );

            ElevatorA_SetTargetFloor(call_floor);

            } else {

                MasterLog_DMA(
                    ">>> EVENT: [CASE 3] PERFECT MATCH "
                    "(Both Match! B is closer & dispatched)\r\n"
                );

                MasterTxPacket.TargetFloor = call_floor;
            }

        __enable_irq();
        return;
    }

    /* ======================================
         * CASE 6: Nearest Idle Elevator
         * ====================================== */
    if (ctxA.currentState == 0 && rxPktB.State == 0) {
        if (abs((int)ctxA.currentFloor - call_floor) <= abs((int)rxPktB.CurrentFloor - call_floor)) {
            MasterLog_DMA(">>> EVENT: [CASE 6] NEAREST IDLE (Both IDLE! A is closer & dispatched)\r\n");
            ElevatorA_SetTargetFloor(call_floor);

        } else {
            MasterLog_DMA(">>> EVENT: [CASE 6] NEAREST IDLE (Both IDLE! B is closer & dispatched)\r\n");
            MasterTxPacket.TargetFloor = call_floor;
        }

        __enable_irq();
        return;
    }

    /* Only Elevator A is idle */
    else if (ctxA.currentState == 0) {

        MasterLog_DMA(
            ">>> EVENT: [CASE 6] NEAREST IDLE "
            "(A dispatched)\r\n"
        );

        ElevatorA_SetTargetFloor(call_floor);

        __enable_irq();
        return;
    }

    /* Only Elevator B is idle */
    else if (rxPktB.State == 0) {

        MasterLog_DMA(
            ">>> EVENT: [CASE 6] NEAREST IDLE "
            "(B dispatched)\r\n"
        );

        MasterTxPacket.TargetFloor = call_floor;

        __enable_irq();
        return;
    }

    /* ======================================
     * CASE 4 & 5:
     * Passed Match / Opposite Direction
     * ====================================== */

    MasterLog_DMA(
        ">>> EVENT: [CASE 4/5] PASSED OR "
        "OPPOSITE DIR (Added to Pending Queue)\r\n"
    );

    PendingCalls[call_floor] = call_dir;

    __enable_irq();
}

/* ========================================== */
/* Pending Queue Update                       */
/* ========================================== */

/**
 * @brief Check pending requests and dispatch
 * them when an elevator becomes idle.
 */
void MasterLogic_Update(void)
{
    ElevatorContextA_t ctxA =
        ElevatorA_GetContext();

    IpcPacket_t rxPktB =
        Ipc_MasterGetRxPacket();

    int i;

    for (i = 1; i <= 4; i++) {

        if (PendingCalls[i] != 0) {

            /*
             * Dispatch pending request when
             * any elevator becomes idle.
             */
            if (ctxA.currentState == 0 ||
                rxPktB.State == 0) {

                int dir = PendingCalls[i];

                PendingCalls[i] = 0;

                MasterLogic_DispatchCall(i, dir);
            }
        }
    }
}

/* ========================================== */
/* Hall Button Interrupt Callbacks            */
/* ========================================== */

/**
 * @brief Floor 1 Up button callback.
 */
void MasterLogic_F1_Up_Callback(void)
{
    MasterLogic_DispatchCall(1, 1);
}

/**
 * @brief Floor 2 Up button callback.
 */
void MasterLogic_F2_Up_Callback(void)
{
    MasterLogic_DispatchCall(2, 1);
}

/**
 * @brief Floor 2 Down button callback.
 */
void MasterLogic_F2_Down_Callback(void)
{
    MasterLogic_DispatchCall(2, -1);
}

/**
 * @brief Floor 3 Up button callback.
 */
void MasterLogic_F3_Up_Callback(void)
{
    MasterLogic_DispatchCall(3, 1);
}

/**
 * @brief Floor 3 Down button callback.
 */
void MasterLogic_F3_Down_Callback(void)
{
    MasterLogic_DispatchCall(3, -1);
}

/**
 * @brief Floor 4 Down button callback.
 */
void MasterLogic_F4_Down_Callback(void)
{
    MasterLogic_DispatchCall(4, -1);
}