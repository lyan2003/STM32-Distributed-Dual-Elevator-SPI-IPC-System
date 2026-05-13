#include "Telemetry.h"
#include "Usart.h"
#include "Rcc.h"
#include "Gpio.h"
#include "elevator.h"
#include "ElevatorB.h"
#include "Ipc.h"
#include "Dma.h"
#include <stdio.h>
#include <string.h>
#include "stm32f401xe.h"

/* Large buffer to hold the report before handing it to the DMA */
char Telemetry_Buffer[512];

/* DMA configuration for USART1 Transmission */
Dma_ConfigType UartTxDma = {
    .ControllerId = DMA_ID_2,           /* USART1_TX is mapped to DMA2 */
    .StreamNum    = 7,                  /* Stream 7 */
    .ChannelNum   = 4,                  /* Channel 4 */
    .Direction    = DMA_MEM_TO_PERI,    /* Memory to Peripheral (UART) */
    .PeriAddr     = 0x40011004,         /* Address of USART1 DR register */
    .Priority     = 1                   /* Medium priority */
};

const char* GetStateStr(uint8 state) {
    switch (state) {
        case 0: return "IDLE       ";
        case 1: return "MOVING UP  ";
        case 2: return "MOVING DOWN";
        case 3: return "DOORS OPEN ";
        case 4: return "EMERGENCY  ";
        default:return "UNKNOWN    ";
    }
}

/* ------------------------------------------------------------------ */
/* Init                                                              */
/* ------------------------------------------------------------------ */
void Telemetry_Init(uint8 isMasterFlag)
{
    /* 1. Enable Clocks */
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_USART1);

    /* === Modification: Force enable DMA2 clock === */
    RCC->AHB1ENR |= (1 << 22);

    /* 2. Configure USART Pins (PA9 = TX, PA10 = RX) */
    Gpio_Init(GPIO_A, 9, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_ConfigAF(GPIO_A, 9, 7);

    Gpio_Init(GPIO_A, 10, GPIO_AF, GPIO_PULL_UP);
    Gpio_ConfigAF(GPIO_A, 10, 7);

    /* 3. Initialize USART */
    Usart1_Init();

    /* === Modification: Enable UART to use DMA === */
    USART1->CR3 |= (1 << 7); /* Enable DMAT (DMA Transmit Enable) bit */

    /* 4. Initialize DMA Stream */
    Dma_InitStream(&UartTxDma);

    /* 5. Startup Message using DMA */
    sprintf(Telemetry_Buffer, "\r\n================================\r\n=== %s MCU STARTUP OK! ===\r\n================================\r\n",
            isMasterFlag ? "MASTER" : "SLAVE ");

    DMA2_Stream7->CR &= ~1;               /* 1. Disable DMA to allow configuration changes */
    while(DMA2_Stream7->CR & 1);          /* Wait until it is effectively disabled */

    DMA2->HIFCR = (0x3D << 22);           /* 2. Clear all old flags (Crucial to prevent DMA from hanging) */

    DMA2_Stream7->M0AR = (uint32)Telemetry_Buffer; /* 3. Buffer address */
    DMA2_Stream7->NDTR = strlen(Telemetry_Buffer); /* 4. Message length */

    USART1->DR = '\r';                    /* 5. Proteus workaround: Send dummy byte without blocking the CPU */

    DMA2_Stream7->CR |= 1;                /* 6. Start the DMA! */
}

/* ------------------------------------------------------------------ */
/* Run                                                               */
/* ------------------------------------------------------------------ */

void Telemetry_Run(uint8 isMasterFlag)
{
    /* Guard: Check if DMA is currently busy.
     * This protects against memory corruption while transferring!
     */
    if (DMA2_Stream7->CR & 1)
    {
        return;
    }

    /* Software Non-Blocking Timer Counter */
    static uint32 telemetry_delay_counter = 0;
    telemetry_delay_counter++;

    /* Flag to decide if we fire the DMA this loop */
    uint8 trigger_print = 0;

    if (isMasterFlag)
    {
        static uint8 last_A_state = 0xFF, last_A_curr = 0xFF, last_A_tgt = 0xFF;
        static uint8 last_B_state = 0xFF, last_B_curr = 0xFF, last_B_tgt = 0xFF;
        static uint8 last_link = 0xFF;

        ElevatorContextA_t ctxA = ElevatorA_GetContext();
        IpcPacket_t rxPkt = Ipc_MasterGetRxPacket();
        uint8 current_link = Ipc_MasterLinkOk();

        /* CONDITION 1: Event-Driven (State Changed) */
        if (ctxA.currentState != last_A_state || ctxA.currentFloor != last_A_curr || ctxA.targetFloor != last_A_tgt ||
            rxPkt.State != last_B_state || rxPkt.CurrentFloor != last_B_curr || rxPkt.TargetFloor != last_B_tgt ||
            current_link != last_link)
        {
            last_A_state = ctxA.currentState; last_A_curr = ctxA.currentFloor; last_A_tgt = ctxA.targetFloor;
            last_B_state = rxPkt.State; last_B_curr = rxPkt.CurrentFloor; last_B_tgt = rxPkt.TargetFloor;
            last_link = current_link;

            trigger_print = 1; /* Force an instant print */
        }

        /* CONDITION 2: Auto-Print (500ms elapsed) */
        if (telemetry_delay_counter >= 150000)
        {
            trigger_print = 1;
        }

        /* Execute the DMA Transfer if either condition was met */
        if (trigger_print)
        {
            /* Reset the timer */
            telemetry_delay_counter = 0;

            /* Assemble the entire report into the buffer at once */
            sprintf(Telemetry_Buffer,
                "\r\n===================================\r\n"
                "[A] %s | Floor: %d -> %d\r\n"
                "[B] %s | Floor: %d -> %d\r\n"
                "IPC Link: %s\r\n"
                "===================================\r\n",
                GetStateStr(ctxA.currentState), ctxA.currentFloor, ctxA.targetFloor,
                GetStateStr(rxPkt.State), rxPkt.CurrentFloor, rxPkt.TargetFloor,
                current_link ? "OK" : "FAULT");

            /* Start DMA for the MASTER (Your exact working code) */
            DMA2_Stream7->CR &= ~1;
            while(DMA2_Stream7->CR & 1);
            DMA2->HIFCR = (0x3D << 22); /* Clear flags */
            DMA2_Stream7->M0AR = (uint32)Telemetry_Buffer;
            DMA2_Stream7->NDTR = strlen(Telemetry_Buffer);
            USART1->DR = '\r'; /* The Proteus Hack! */
            DMA2_Stream7->CR |= 1;
        }
    }
    else
    {
        /* SLAVE BOARD */
        static uint8 last_B_state = 0xFF, last_B_curr = 0xFF, last_B_tgt = 0xFF;
        ElevatorContextB_t ctxB = ElevatorB_GetContext();

        /* CONDITION 1: State Changed */
        if (ctxB.currentState != last_B_state || ctxB.currentFloor != last_B_curr || ctxB.targetFloor != last_B_tgt)
        {
            last_B_state = ctxB.currentState; last_B_curr = ctxB.currentFloor; last_B_tgt = ctxB.targetFloor;
            trigger_print = 1;
        }

        /* CONDITION 2: Auto*/
        if (telemetry_delay_counter >= 150000)
        {
            trigger_print = 1;
        }

        if (trigger_print)
        {
            telemetry_delay_counter = 0;

            /* Assemble the entire report into the buffer at once */
            sprintf(Telemetry_Buffer,
                "\r\n--- SLAVE UPDATE ---\r\n"
                "[B] %s | Floor: %d -> %d\r\n"
                "--------------------\r\n",
                GetStateStr(ctxB.currentState), ctxB.currentFloor, ctxB.targetFloor);

            /* Start DMA for the SLAVE */
            DMA2_Stream7->CR &= ~1;
            while(DMA2_Stream7->CR & 1);
            DMA2->HIFCR = (0x3D << 22); /* Clear flags */
            DMA2_Stream7->M0AR = (uint32)Telemetry_Buffer;
            DMA2_Stream7->NDTR = strlen(Telemetry_Buffer);
            USART1->DR = '\r';
            DMA2_Stream7->CR |= 1;
        }
    }
}