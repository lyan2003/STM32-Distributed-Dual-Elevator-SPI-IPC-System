#include "Spi.h"
#include "Gpio.h"
#include "Rcc.h"
#include "Nvic.h"

/*
 * Pointer to user-defined callback function.
 * Used only in Slave mode when data is received.
 */
static Spi_RxCallback_t SlaveRxCallback = NULL;
/* Variables for Master IT communication */
static uint8* MasterTxBuffer = NULL;
static uint8* MasterRxBuffer = NULL;
static uint16 MasterTransferLength = 0;
static volatile uint16 MasterTxIndex = 0;
static volatile uint16 MasterRxIndex = 0;
static volatile uint8 MasterBusyFlag = 0; // 0 = Idle, 1 = Busy
static uint8 CurrentSpiMode = SPI_MASTER; // To know what to do in ISR

/* Master complete callback */
typedef void (*Spi_MasterDoneCallback_t)(void);
static Spi_MasterDoneCallback_t MasterTxRxDoneCallback = NULL;


/*
 * SPI4 Initialization
 * ----------------------------------------------------------
 * Configures SPI4 peripheral as either Master or Slave.
 *
 * Parameters:
 *  MasterSlave -> SPI_MASTER or SPI_SLAVE
 *  ClkPol      -> Clock polarity
 *  ClkPhase    -> Clock phase
 */
void Spi4_Init(uint8 MasterSlave, uint8 ClkPol, uint8 ClkPhase)
{
    // ======================================================
    // 1. Enable Peripheral Clocks
    // ======================================================

    /* Enable GPIOE clock */
    Rcc_Enable(RCC_GPIOE);

    /* Enable SPI4 peripheral clock */
    Rcc_Enable(RCC_SPI4);


    // ======================================================
    // 2. Configure SPI Pins
    // ======================================================

    /*
     * PE2 -> SCK
     * PE5 -> MISO
     * PE6 -> MOSI
     */

    Gpio_Init(GPIO_E, 2, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_Init(GPIO_E, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_Init(GPIO_E, 6, GPIO_AF, GPIO_PUSH_PULL);

    /*
     * Configure Alternate Function AF5 for SPI4
     */
    Gpio_ConfigAF(GPIO_E, 2, 5);
    Gpio_ConfigAF(GPIO_E, 5, 5);
    Gpio_ConfigAF(GPIO_E, 6, 5);


    // ======================================================
    // 3. Configure Master / Slave Mode
    // ======================================================

    if (MasterSlave == SPI_MASTER)
    {
        // --------------------------------------------------
        // MASTER CONFIGURATION
        // --------------------------------------------------
        CurrentSpiMode = SPI_MASTER; // Save current mode
        /*
         * PE4 used as software-controlled Chip Select (CS)
         */
        Gpio_Init(GPIO_E, 4, GPIO_OUTPUT, GPIO_PUSH_PULL);

        /* Keep CS inactive initially */
        Gpio_WritePin(GPIO_E, 4, 1);

        /* Configure SPI as Master */
        SPI_REG->CR1 |= (1 << 2);     // MSTR = 1

        /*
         * Enable Software Slave Management
         * Required when controlling NSS manually
         */
        SPI_REG->CR1 |= (1 << 9);     // SSM = 1
        SPI_REG->CR1 |= (1 << 8);     // SSI = 1

        /*
         * Configure baud rate:
         * SPI Clock = Peripheral Clock / 16
         */
        SPI_REG->CR1 |= (0x03 << 3);

        /* Enable SPI4 interrupt in NVIC for Master */
        Nvic_SetPriority(84, 1);
        Nvic_EnableIrq(84);
    }
    else
    {
        // --------------------------------------------------
        // SLAVE CONFIGURATION
        // --------------------------------------------------
        CurrentSpiMode = SPI_SLAVE;
        /*
         * PE4 configured as hardware NSS input
         */
        Gpio_Init(GPIO_E, 4, GPIO_AF, GPIO_PUSH_PULL);
        Gpio_ConfigAF(GPIO_E, 4, 5);

        /* Configure SPI as Slave */
        SPI_REG->CR1 &= ~(1 << 2);    // MSTR = 0

        /*
         * Disable Software Slave Management
         * NSS handled by hardware
         */
        SPI_REG->CR1 &= ~(1 << 9);    // SSM = 0

        /*
         * Enable RXNE interrupt:
         * Trigger interrupt whenever data is received
         */
        SPI_REG->CR2 |= (1 << 6);     // RXNEIE = 1

        /* Configure and enable SPI4 interrupt in NVIC */
        Nvic_SetPriority(84, 1);      // SPI4 IRQ number = 84
        Nvic_EnableIrq(84);
    }


    // ======================================================
    // 4. Configure Clock Polarity & Clock Phase
    // ======================================================

    /*
     * Clock Polarity (CPOL)
     */
    if (ClkPol == SPI_IDLE_HIGH)
    {
        SPI_REG->CR1 |= (1 << 1);
    }
    else
    {
        SPI_REG->CR1 &= ~(1 << 1);
    }

    /*
     * Clock Phase (CPHA)
     */
    if (ClkPhase == SPI_SAMPLE_SECOND_TRANSITION)
    {
        SPI_REG->CR1 |= (1 << 0);
    }
    else
    {
        SPI_REG->CR1 &= ~(1 << 0);
    }


    // ======================================================
    // 5. Enable SPI Peripheral
    // ======================================================

    SPI_REG->CR1 |= (1 << 6); // SPE = 1
}


/*
 * Master Transmit / Receive (Non-Blocking Interrupt-Based)
 * ----------------------------------------------------------
 * Starts a full-duplex SPI transfer using RXNE interrupts only.
 *
 * Parameters:
 *   txBuf    -> Pointer to transmit buffer
 *   rxBuf    -> Pointer to receive buffer
 *   length   -> Number of bytes to transfer
 *   callback -> Function executed when transfer completes
 *
 * Notes:
 *   - The transfer is interrupt-driven.
 *   - RXNE interrupt is used instead of TXE interrupt
 *     to improve synchronization stability.
 *   - The first byte is transmitted manually to start
 *     SPI clock generation.
 */
void Spi4_MasterTransmitReceive(uint8* txBuf,
                                uint8* rxBuf,
                                uint16 length,
                                Spi_MasterDoneCallback_t callback)
{
    /* Prevent starting a new transfer while SPI is busy */
    if (MasterBusyFlag == 1 || length == 0)
        return;

    /* Mark SPI as busy */
    MasterBusyFlag = 1;

    /* Store transfer configuration */
    MasterTxBuffer = txBuf;
    MasterRxBuffer = rxBuf;
    MasterTransferLength = length;

    /*
     * Transmission starts from index 1 because
     * the first byte will be sent manually below.
     */
    MasterTxIndex = 1;

    /* No received bytes yet */
    MasterRxIndex = 0;

    /* Register completion callback */
    MasterTxRxDoneCallback = callback;

    /*
     * Enable RXNE interrupt only
     * --------------------------------------------------
     * RXNEIE = 1  -> Enable receive interrupt
     * TXEIE  = 0  -> Disable transmit interrupt
     *
     * TXE interrupt is intentionally disabled to avoid
     * synchronization issues and Proteus simulation hangs.
     */
    SPI_REG->CR2 |=  (1 << 6); // RXNEIE
    SPI_REG->CR2 &= ~(1 << 7); // TXEIE

    /*
     * Initial transmission
     * --------------------------------------------------
     * Send the first byte manually to generate the SPI
     * clock and trigger the first RXNE interrupt.
     */
    SPI_REG->DR = MasterTxBuffer[0];
}

/*
 * Control Chip Select (CS)
 * ----------------------------------------------------------
 * Used only in Master mode.
 *
 * Parameters:
 *  state:
 *      0 -> CS Active (Low)
 *      1 -> CS Idle   (High)
 */
void Spi4_MasterSetCS(uint8 state)
{
    Gpio_WritePin(GPIO_E, 4, state);
}


/*
 * Register Slave Receive Callback
 * ----------------------------------------------------------
 * Stores application callback function that will be called
 * whenever SPI slave receives data.
 */
void Spi4_SetSlaveCallback(Spi_RxCallback_t callback)
{
    SlaveRxCallback = callback;
}


/*
 * Preload Slave Data Register
 * ----------------------------------------------------------
 * Loads next byte into SPI Data Register before master
 * generates the next clock pulse.
 *
 * Important:
 * SPI slave must always prepare outgoing data in advance.
 */
void Spi4_SlavePreloadData(uint8 TxData)
{
    SPI_REG->DR = TxData;
}


/*
 * SPI4 Interrupt Handler
 * ----------------------------------------------------------
 * This ISR is triggered automatically whenever an SPI4
 * interrupt occurs.
 *
 * Supports:
 *   - Slave mode reception
 *   - Master mode full-duplex transfer
 */
void SPI4_IRQHandler(void)
{
    if (CurrentSpiMode == SPI_SLAVE)
    {
        /* --------------------------------------------------
         * SLAVE MODE
         * --------------------------------------------------
         * Handle received data from the master.
         */
        if (SPI_REG->SR & (1 << 0)) // RXNE flag set
        {
            /* Read received byte */
            uint8 rxData = SPI_REG->DR;

            /* Execute registered callback if available */
            if (SlaveRxCallback != NULL)
            {
                SlaveRxCallback(rxData);
            }
        }
    }
    else
    {
        /* --------------------------------------------------
         * MASTER MODE
         * --------------------------------------------------
         * RXNE-driven full-duplex communication.
         */
        if (SPI_REG->SR & (1 << 0)) // RXNE flag set
        {
            /* Store received byte from slave */
            MasterRxBuffer[MasterRxIndex++] = SPI_REG->DR;

            /* Check if more bytes remain to be transmitted */
            if (MasterTxIndex < MasterTransferLength)
            {
                /*
                 * Small synchronization delay
                 * --------------------------------------------------
                 * Gives the slave enough time to:
                 *   1. Enter its interrupt handler
                 *   2. Process the received byte
                 *   3. Prepare the next response byte
                 *
                 * Without this delay, the master may clock the next
                 * transfer too quickly, causing invalid or repeated
                 * slave data.
                 */
                for (volatile int d = 0; d < 300; d++);

                /* Transmit next byte */
                SPI_REG->DR = MasterTxBuffer[MasterTxIndex++];
            }
            else
            {
                /*
                 * Transmission completed
                 * --------------------------------------------------
                 * All expected bytes have been transmitted and
                 * received successfully.
                 */
                if (MasterRxIndex == MasterTransferLength)
                {
                    /* Disable RXNE interrupt */
                    SPI_REG->CR2 &= ~(1 << 6);

                    /* Clear busy flag */
                    MasterBusyFlag = 0;

                    /* Execute completion callback if registered */
                    if (MasterTxRxDoneCallback != NULL)
                    {
                        MasterTxRxDoneCallback();
                    }
                }
            }
        }
    }
}