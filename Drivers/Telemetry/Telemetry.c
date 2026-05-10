#include "Telemetry.h"
#include "Usart.h"
#include "Rcc.h"
#include "Gpio.h"
#include "elevator.h"
#include "ElevatorB.h"
#include "Ipc.h"

/* ------------------------------------------------------------------ */
/* Init                                                              */
/* ------------------------------------------------------------------ */

void Telemetry_Init(uint8 isMasterFlag)
{
    /* 1. Enable Clocks */
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_USART1);

    /* 2. Configure USART Pins (PA9 = TX, PA10 = RX) */
    Gpio_Init(GPIO_A, 9, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_ConfigAF(GPIO_A, 9, 7);

    Gpio_Init(GPIO_A, 10, GPIO_AF, GPIO_PULL_UP);
    Gpio_ConfigAF(GPIO_A, 10, 7);

    /* 3. Initialize USART */
    Usart1_Init();

    /* 4. Startup Message */
    Usart1_TransmitString("\r\n================================\r\n");
    if (isMasterFlag) {
        Usart1_TransmitString("=== MASTER MCU STARTUP OK! ===\r\n");
    } else {
        Usart1_TransmitString("=== SLAVE MCU STARTUP OK!  ===\r\n");
    }
    Usart1_TransmitString("================================\r\n");
}

/* ------------------------------------------------------------------ */
/* Run                                                               */
/* ------------------------------------------------------------------ */

void Telemetry_Run(uint8 isMasterFlag)
{
    if (isMasterFlag)
    {
        /* Memory variables to detect changes */
        static uint8 last_A_state = 0xFF, last_A_curr = 0xFF, last_A_tgt = 0xFF;
        static uint8 last_B_state = 0xFF, last_B_curr = 0xFF, last_B_tgt = 0xFF;
        static uint8 last_link = 0xFF;

        /* Get current states */
        ElevatorContextA_t ctxA = ElevatorA_GetContext();
        IpcPacket_t rxPkt = Ipc_MasterGetRxPacket();
        uint8 current_link = Ipc_MasterLinkOk();

        /* Print ONLY if something changed */
        if (ctxA.currentState != last_A_state || ctxA.currentFloor != last_A_curr || ctxA.targetFloor != last_A_tgt ||
            rxPkt.State != last_B_state || rxPkt.CurrentFloor != last_B_curr || rxPkt.TargetFloor != last_B_tgt ||
            current_link != last_link)
        {
            /* Update Memory */
            last_A_state = ctxA.currentState;
            last_A_curr = ctxA.currentFloor;
            last_A_tgt = ctxA.targetFloor;
            last_B_state = rxPkt.State;
            last_B_curr = rxPkt.CurrentFloor;
            last_B_tgt = rxPkt.TargetFloor;
            last_link = current_link;

            Usart1_TransmitString("\r\n===================================\r\n");

            /* --- Print Elevator A --- */
            Usart1_TransmitString("[A] ");
            switch (ctxA.currentState) {
                case 0: Usart1_TransmitString("IDLE       "); break;
                case 1: Usart1_TransmitString("MOVING UP  "); break;
                case 2: Usart1_TransmitString("MOVING DOWN"); break;
                case 3: Usart1_TransmitString("DOORS OPEN "); break;
                case 4: Usart1_TransmitString("EMERGENCY  "); break;
                default:Usart1_TransmitString("UNKNOWN    "); break;
            }
            Usart1_TransmitString(" | Floor: ");
            Usart1_TransmitByte(ctxA.currentFloor + '0');
            Usart1_TransmitString(" -> ");
            Usart1_TransmitByte(ctxA.targetFloor + '0');
            Usart1_TransmitString("\r\n");

            /* --- Print Elevator B --- */
            Usart1_TransmitString("[B] ");
            switch (rxPkt.State) {
                case 0: Usart1_TransmitString("IDLE       "); break;
                case 1: Usart1_TransmitString("MOVING UP  "); break;
                case 2: Usart1_TransmitString("MOVING DOWN"); break;
                case 3: Usart1_TransmitString("DOORS OPEN "); break;
                case 4: Usart1_TransmitString("EMERGENCY  "); break;
                default:Usart1_TransmitString("UNKNOWN    "); break;
            }
            Usart1_TransmitString(" | Floor: ");
            Usart1_TransmitByte(rxPkt.CurrentFloor + '0');
            Usart1_TransmitString(" -> ");
            Usart1_TransmitByte(rxPkt.TargetFloor + '0');
            Usart1_TransmitString("\r\n");

            /* --- Print Link Status --- */
            Usart1_TransmitString("IPC Link: ");
            if (current_link) Usart1_TransmitString("OK\r\n");
            else              Usart1_TransmitString("FAULT\r\n");

            Usart1_TransmitString("===================================\r\n");
        }
    }
    else
    {
        /* ==========================================================
         * SLAVE BOARD
         * ========================================================== */
        static uint8 last_B_state = 0xFF, last_B_curr = 0xFF, last_B_tgt = 0xFF;
        ElevatorContextB_t ctxB = ElevatorB_GetContext();

        if (ctxB.currentState != last_B_state || ctxB.currentFloor != last_B_curr || ctxB.targetFloor != last_B_tgt)
        {
            last_B_state = ctxB.currentState;
            last_B_curr = ctxB.currentFloor;
            last_B_tgt = ctxB.targetFloor;

            Usart1_TransmitString("\r\n--- SLAVE UPDATE ---\r\n");
            Usart1_TransmitString("[B] ");
            switch (ctxB.currentState) {
                case 0: Usart1_TransmitString("IDLE       "); break;
                case 1: Usart1_TransmitString("MOVING UP  "); break;
                case 2: Usart1_TransmitString("MOVING DOWN"); break;
                case 3: Usart1_TransmitString("DOORS OPEN "); break;
                case 4: Usart1_TransmitString("EMERGENCY  "); break;
                default:Usart1_TransmitString("UNKNOWN    "); break;
            }
            Usart1_TransmitString(" | Floor: ");
            Usart1_TransmitByte(ctxB.currentFloor + '0');
            Usart1_TransmitString(" -> ");
            Usart1_TransmitByte(ctxB.targetFloor + '0');
            Usart1_TransmitString("\r\n--------------------\r\n");
        }
    }
}