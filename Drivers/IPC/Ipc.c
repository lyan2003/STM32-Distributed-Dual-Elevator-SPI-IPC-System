#include "Ipc.h"
#include "Spi.h"
#include "ElevatorB.h"   // Slave elevator controller
#include "Rcc.h"
#include "Timer.h"

// ==========================================================
// Global Variables - Slave Side
// ==========================================================

// Packet transmitted from Slave (Elevator B) to Master
static IpcPacket_t SlaveTxPacket;

// Buffer used to store received bytes from Master
static uint8 SlaveRxBuffer[IPC_PACKET_SIZE];

// Tracks current byte index during SPI frame reception
static uint8 byteIndex = 0;


// ==========================================================
// 1. Slave Implementation (Elevator B)
// ==========================================================

/*
 * SPI Slave Receive Callback
 * ----------------------------------------------------------
 * This callback is triggered every time the slave receives
 * one byte from the master.
 *
 * Responsibilities:
 * 1. Store incoming byte in reception buffer.
 * 2. Detect completion of full IPC frame.
 * 3. Extract target floor command from master.
 * 4. Preload next byte to be transmitted back to master.
 */
static void Ipc_SlaveRxCallback(uint8 rxData)
{
    /* Store received byte in RX buffer */
    SlaveRxBuffer[byteIndex] = rxData;

    /* Move to next byte position */
    byteIndex++;

    /*
     * If full packet is received:
     * - Reset index
     * - Validate header
     * - Process command from master
     */
    if (byteIndex >= IPC_PACKET_SIZE)
    {
        byteIndex = 0;

        /* Validate packet header */
        if (SlaveRxBuffer[0] == IPC_HEADER_BYTE)
        {
            uint8 targetFromMaster = SlaveRxBuffer[3];

            /*
             * Valid floor range:
             * Floors 1 -> 4
             */
            if ((targetFromMaster != 0) && (targetFromMaster <= 4))
            {
                /* Forward request to Elevator B controller */
                ElevatorB_SetTargetFromSPI(targetFromMaster);
            }
        }
    }

    /*
     * Preload next byte into SPI Data Register.
     * The master receives this byte during the next SPI clock.
     */
    uint8* txPtr = (uint8*)&SlaveTxPacket;
    Spi4_SlavePreloadData(txPtr[byteIndex]);
}


/*
 * Initialize IPC Slave
 * ----------------------------------------------------------
 * Configures SPI4 in Slave mode and prepares the initial
 * response packet sent to the master.
 */
void Ipc_SlaveInit(void)
{
    /* Initialize default packet values */
    SlaveTxPacket.Header        = IPC_HEADER_BYTE;
    SlaveTxPacket.State         = 0;
    SlaveTxPacket.CurrentFloor  = 1;
    SlaveTxPacket.TargetFloor   = 1;
    SlaveTxPacket.Emergency     = 0;
    SlaveTxPacket.Reserved1     = 0;
    SlaveTxPacket.Reserved2     = 0;

    /* Initial checksum calculation */
    SlaveTxPacket.Checksum = IPC_HEADER_BYTE + 1 + 1;

    /* Configure SPI4 as Slave */
    Spi4_Init(SPI_SLAVE,
              SPI_IDLE_LOW,
              SPI_SAMPLE_FIRST_TRANSITION);

    /* Register receive callback */
    Spi4_SetSlaveCallback(Ipc_SlaveRxCallback);

    /*
     * Load first byte before communication starts.
     * Required because SPI slave must always have data ready.
     */
    Spi4_SlavePreloadData(SlaveTxPacket.Header);
}


/*
 * Update Slave State Packet
 * ----------------------------------------------------------
 * Refreshes the packet fields according to Elevator B status.
 * This packet will later be transmitted to the master.
 */
void Ipc_SlaveUpdateState(ElevatorContextB_t ctx)
{
    /* Update elevator runtime state */
    SlaveTxPacket.State        = (uint8)ctx.currentState;
    SlaveTxPacket.CurrentFloor = ctx.currentFloor;
    SlaveTxPacket.TargetFloor  = ctx.targetFloor;

    /* Emergency flag handling */
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


// ==========================================================
// 2. Master Implementation (Elevator A)
// ==========================================================

/*
 * Initialize IPC Master
 * ----------------------------------------------------------
 * Configures SPI4 in Master mode.
 */
void Ipc_MasterInit(void)
{
    /* Configure SPI4 as Master */
    Spi4_Init(SPI_MASTER,
              SPI_IDLE_LOW,
              SPI_SAMPLE_FIRST_TRANSITION);

    /* Enable timer used for communication delays */
    Rcc_Enable(RCC_TIM3);
}


/*
 * Exchange State Between Master and Slave
 * ----------------------------------------------------------
 * Sends one complete packet from Master to Slave while
 * simultaneously receiving a packet from Slave.
 *
 * Parameters:
 *  txPacket -> Packet sent by master
 *  rxPacket -> Packet received from slave
 *
 * Return:
 *  1 -> Valid packet received
 *  0 -> Invalid packet / checksum failure
 */
uint8 Ipc_MasterExchangeState(IpcPacket_t* txPacket,
                              IpcPacket_t* rxPacket)
{
    uint8* txPtr = (uint8*)txPacket;
    uint8* rxPtr = (uint8*)rxPacket;

    uint8 calculatedChecksum = 0;

    /* Generate checksum before transmission */
    txPacket->Checksum =
        txPacket->Header +
        txPacket->State +
        txPacket->CurrentFloor +
        txPacket->TargetFloor +
        txPacket->Emergency +
        txPacket->Reserved1 +
        txPacket->Reserved2;

    /*
     * Pull CS low to select and wake up slave.
     * Required before starting SPI transfer.
     */
    Spi4_MasterSetCS(0);

    /*
     * Small delay allows slave hardware/software
     * to detect NSS transition properly.
     * Especially important in Proteus simulations.
     */
    Timer_DelayMs(TIMER3, 1);

    /*
     * Full-duplex SPI transfer:
     * Every transmitted byte simultaneously receives one byte.
     */
    for (uint8 i = 0; i < IPC_PACKET_SIZE; i++)
    {
        rxPtr[i] = Spi4_MasterTransmitReceive(txPtr[i]);
    }

    /* Release slave by setting CS high */
    Spi4_MasterSetCS(1);

    /*
     * Validate received packet:
     * 1. Verify header
     * 2. Verify checksum
     */
    if (rxPacket->Header == IPC_HEADER_BYTE)
    {
        for (uint8 i = 0; i < 7; i++)
        {
            calculatedChecksum += rxPtr[i];
        }

        if (calculatedChecksum == rxPacket->Checksum)
        {
            return 1;   // Packet is valid
        }
    }

    return 0;   // Packet corrupted or invalid
}