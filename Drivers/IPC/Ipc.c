#include "Ipc.h"
#include "Spi.h"
#include "Rcc.h"
#include "Timer.h"

/* ------------------------------------------------------------------ */
/* IPC Master State Machine                                           */
/* ------------------------------------------------------------------ */
typedef enum {
    IPC_IDLE = 0,
    IPC_CS_ASSERT,
    IPC_TRANSFERRING,
    IPC_CS_RELEASE
} IpcMasterState_t;

/* Current state of the IPC master */
static volatile IpcMasterState_t MasterState = IPC_IDLE;

/* Last received packet from slave */
IpcPacket_t MasterRxPacket;

/* Communication link status flag */
uint8 Ipc_LastLinkOk = 0;

/* ------------------------------------------------------------------ */
/* Timer Callback Functions                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Triggered every 50ms to start a new SPI transaction.
 */
static void Ipc_50ms_Callback(void) {
    if (MasterState == IPC_IDLE) {
        MasterState = IPC_CS_ASSERT;
    }
}

/**
 * @brief Callback used after CS setup delay.
 */
static void Ipc_CsSetup_Callback(void) {
    MasterState = IPC_TRANSFERRING;
}

/* ------------------------------------------------------------------ */
/* Master Functions                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize IPC master module.
 *
 * Configures SPI4 as master and starts the periodic trigger timer.
 */
void Ipc_MasterInit(void) {

    /* Enable clocks for timers used by IPC */
    Rcc_Enable(RCC_TIM2);
    Rcc_Enable(RCC_TIM5);

    /* Initialize SPI4 in master mode */
    Spi4_Init(SPI_MASTER,
              SPI_IDLE_LOW,
              SPI_SAMPLE_FIRST_TRANSITION);

    /* Start periodic 50ms trigger */
    Timer_DelayMsAsync(TIMER2, 50, Ipc_50ms_Callback);
}

/**
 * @brief Perform SPI packet exchange with the slave.
 *
 * @param txPacket Pointer to the packet to transmit.
 */
void Ipc_MasterRun(IpcPacket_t* txPacket)
{
    /* ---------------------------------------------------------
     * Software-based non-blocking delay
     * Controls how often SPI communication is executed.
     * --------------------------------------------------------- */
    static uint32 spi_delay_counter = 0;

    spi_delay_counter++;

    if (spi_delay_counter < 15000) {
        return;
    }

    spi_delay_counter = 0;

    /* ---------------------------------------------------------
     * SPI Packet Exchange
     * --------------------------------------------------------- */

    uint8* txPtr = (uint8*)txPacket;
    uint8* rxPtr = (uint8*)&MasterRxPacket;

    uint8 calculatedChecksum = 0;

    /* Calculate outgoing packet checksum */
    txPacket->Checksum =
        txPacket->Header +
        txPacket->State +
        txPacket->CurrentFloor +
        txPacket->TargetFloor +
        txPacket->Emergency +
        txPacket->Reserved1 +
        txPacket->Reserved2;

    /* Assert chip select (active low) */
    Spi4_MasterSetCS(0);

    /* Small hardware settle delay */
    for (volatile int i = 0; i < 150; i++);

    /* Exchange all packet bytes */
    for (uint8 i = 0; i < IPC_PACKET_SIZE; i++) {

        rxPtr[i] = Spi4_MasterTransmitReceive(txPtr[i]);

        /* Small spacing delay between bytes */
        for (volatile int d = 0; d < 250; d++);
    }

    /* Release chip select */
    Spi4_MasterSetCS(1);

    /* ---------------------------------------------------------
     * Validate received packet
     * --------------------------------------------------------- */

    if (MasterRxPacket.Header == IPC_HEADER_BYTE) {

        /* Recalculate checksum */
        for (uint8 i = 0; i < 7; i++) {
            calculatedChecksum += rxPtr[i];
        }

        /* Verify checksum */
        if (calculatedChecksum == MasterRxPacket.Checksum) {
            Ipc_LastLinkOk = 1;
        } else {
            Ipc_LastLinkOk = 0;
        }

    } else {

        /* Invalid header detected */
        Ipc_LastLinkOk = 0;
    }
}

/**
 * @brief Get current communication status.
 *
 * @return 1 if last communication was valid, otherwise 0.
 */
uint8 Ipc_MasterLinkOk(void) {
    return Ipc_LastLinkOk;
}

/**
 * @brief Get the latest packet received from the slave.
 *
 * @return Last received slave packet.
 */
IpcPacket_t Ipc_MasterGetRxPacket(void) {
    return MasterRxPacket;
}

/* ------------------------------------------------------------------ */
/* Slave Functions (RXNE Interrupt Driven)                            */
/* ------------------------------------------------------------------ */

/* Packet transmitted from slave to master */
static IpcPacket_t SlaveTxPacket;

/* Receive buffer for incoming master packet */
static uint8 SlaveRxBuffer[IPC_PACKET_SIZE];

/* Current byte index during SPI reception */
static uint8 byteIndex = 0;

/**
 * @brief SPI RX callback executed on every received byte.
 *
 * @param rxData Received SPI byte.
 */
static void Ipc_SlaveRxCallback(uint8 rxData)
{
    /* Store received byte */
    SlaveRxBuffer[byteIndex++] = rxData;

    /* Packet completely received */
    if (byteIndex >= IPC_PACKET_SIZE) {

        byteIndex = 0;

        /* Validate packet header */
        if (SlaveRxBuffer[0] == IPC_HEADER_BYTE) {

            uint8 tgt = SlaveRxBuffer[3];

            /* ------------------------------------------------------
             * Prevent target overwrite while elevator is moving
             * ------------------------------------------------------ */

            if (tgt >= 1 && tgt <= 4) {

                /* Get current elevator state */
                extern ElevatorContextB_t ElevatorB_GetContext(void);
                ElevatorContextB_t ctx = ElevatorB_GetContext();

                /*
                 * Accept a new target only if:
                 * 1. Elevator is idle
                 * 2. Requested floor differs from current floor
                 */
                if (ctx.currentState == 0) {

                    if (tgt != ctx.currentFloor) {
                        ElevatorB_SetTargetFromSPI(tgt);
                    }
                }
            }
        }
    }

    /* Preload next byte for SPI transmission */
    uint8* txPtr = (uint8*)&SlaveTxPacket;
    Spi4_SlavePreloadData(txPtr[byteIndex]);
}

/**
 * @brief Initialize IPC slave module.
 */
void Ipc_SlaveInit(void)
{
    /* Initialize default slave packet values */
    SlaveTxPacket.Header       = IPC_HEADER_BYTE;
    SlaveTxPacket.State        = 0;
    SlaveTxPacket.CurrentFloor = 1;
    SlaveTxPacket.TargetFloor  = 1;
    SlaveTxPacket.Emergency    = 0;
    SlaveTxPacket.Reserved1    = 0;
    SlaveTxPacket.Reserved2    = 0;

    /* Initial checksum */
    SlaveTxPacket.Checksum =
        IPC_HEADER_BYTE + 1 + 1;

    /* Initialize SPI4 in slave mode */
    Spi4_Init(SPI_SLAVE,
              SPI_IDLE_LOW,
              SPI_SAMPLE_FIRST_TRANSITION);

    /* Register RX callback */
    Spi4_SetSlaveCallback(Ipc_SlaveRxCallback);

    /* Preload first transmission byte */
    Spi4_SlavePreloadData(SlaveTxPacket.Header);
}

/**
 * @brief Update slave state packet before transmission.
 *
 * @param ctx Current elevator context.
 */
void Ipc_SlaveUpdateState(ElevatorContextB_t ctx)
{
    SlaveTxPacket.State        = (uint8)ctx.currentState;
    SlaveTxPacket.CurrentFloor = ctx.currentFloor;
    SlaveTxPacket.TargetFloor  = ctx.targetFloor;

    SlaveTxPacket.Emergency =
        (ctx.currentState == STATE_EMERGENCY_B) ? 1 : 0;

    /* Recalculate checksum */
    SlaveTxPacket.Checksum =
        SlaveTxPacket.Header +
        SlaveTxPacket.State +
        SlaveTxPacket.CurrentFloor +
        SlaveTxPacket.TargetFloor +
        SlaveTxPacket.Emergency +
        SlaveTxPacket.Reserved1 +
        SlaveTxPacket.Reserved2;
}