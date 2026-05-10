#include "Rcc.h"
#include "Gpio.h"
#include "elevator.h"
#include "ElevatorB.h"
#include "Ipc.h"
#include "MasterLogic.h"
#include "Telemetry.h"

/* ========================================== */
/* Board Identification Pin                   */
/* ========================================== */

/*
 * PC13 determines board role:
 * 1 -> Master Board
 * 0 -> Slave Board
 */
#define ID_PORT  GPIO_C
#define ID_PIN   13

/* ========================================== */
/* Global IPC Packet                          */
/* ========================================== */

/* Packet transmitted from master to slave */
IpcPacket_t MasterTxPacket;

/* ========================================== */
/* Main Application                           */
/* ========================================== */

int main(void)
{
    /* ======================================
     * Board Identification Initialization
     * ====================================== */

    Rcc_Enable(RCC_GPIOC);

    Gpio_Init(ID_PORT,
              ID_PIN,
              GPIO_INPUT,
              GPIO_PULL_DOWN);

    /* Read board role */
    uint8 isMaster =
        Gpio_ReadPin(ID_PORT, ID_PIN);

    /* ======================================
     * Initialize Telemetry
     * ====================================== */

    Telemetry_Init(isMaster);

    /* ======================================
     * MASTER BOARD
     * ====================================== */

    if (isMaster)
    {
        /* ----------------------------------
         * Initialize Modules
         * ---------------------------------- */

        ElevatorA_Init();

        Ipc_MasterInit();

        MasterLogic_Init();

        /* ----------------------------------
         * Initialize Master TX Packet
         * ---------------------------------- */

        MasterTxPacket.Header       = IPC_HEADER_BYTE;
        MasterTxPacket.TargetFloor  = 0;
        MasterTxPacket.Reserved1    = 0;
        MasterTxPacket.Reserved2    = 0;

        /* ----------------------------------
         * Main Loop
         * ---------------------------------- */

        while (1)
        {
            /* ==============================
             * 1. Run Elevator FSM
             * ============================== */

            ElevatorA_RunFSM();

            /* ==============================
             * 2. Update IPC Packet Data
             * ============================== */

            ElevatorContextA_t ctxA =
                ElevatorA_GetContext();

            MasterTxPacket.State =
                (uint8)ctxA.currentState;

            MasterTxPacket.CurrentFloor =
                ctxA.currentFloor;

            MasterTxPacket.Emergency =
                (ctxA.currentState == STATE_EMERGENCY)
                ? 1 : 0;

            /* ==============================
             * 3. Transmit Data to Slave
             * ============================== */

            Ipc_MasterRun(&MasterTxPacket);

            /* ==============================
             * 4. Handle Pending Requests
             * ============================== */

            MasterLogic_Update();

            /* ==============================
             * 5. Update Telemetry
             * ============================== */

            Telemetry_Run(isMaster);
        }
    }

    /* ======================================
     * SLAVE BOARD
     * ====================================== */

    else
    {
        /* ----------------------------------
         * Initialize Modules
         * ---------------------------------- */

        ElevatorB_Init();

        Ipc_SlaveInit();

        /* ----------------------------------
         * Main Loop
         * ---------------------------------- */

        while (1)
        {
            /* ==============================
             * 1. Get Elevator Context
             * ============================== */

            ElevatorContextB_t ctxB =
                ElevatorB_GetContext();

            /* ==============================
             * 2. Update Slave IPC Packet
             * ============================== */

            Ipc_SlaveUpdateState(ctxB);

            /* ==============================
             * 3. Run Elevator FSM
             * ============================== */

            ElevatorB_RunFSM();

            /* ==============================
             * 4. Update Telemetry
             * ============================== */

            Telemetry_Run(isMaster);
        }
    }

    return 0;
}