#include "Spi.h"
#include "Gpio.h"
#include "Rcc.h"
#include "Nvic.h"

/*
 * Pointer to user-defined callback function.
 * Used only in Slave mode when data is received.
 */
static Spi_RxCallback_t SlaveRxCallback = NULL;


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
    }
    else
    {
        // --------------------------------------------------
        // SLAVE CONFIGURATION
        // --------------------------------------------------

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
 * Master Transmit / Receive
 * ----------------------------------------------------------
 * Performs one full-duplex SPI transfer.
 *
 * Every transmitted byte simultaneously receives one byte.
 *
 * Parameters:
 *  TxData -> Byte to send
 *
 * Return:
 *  Received byte from slave
 */
uint8 Spi4_MasterTransmitReceive(uint8 TxData)
{
    /* Wait until transmit buffer becomes empty */
    while (!(SPI_REG->SR & (1 << 1)));

    /* Write data to Data Register */
    SPI_REG->DR = TxData;

    /* Wait until receive buffer contains data */
    while (!(SPI_REG->SR & (1 << 0)));

    /* Return received byte */
    return SPI_REG->DR;
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
 * Executed automatically when RXNE interrupt occurs.
 *
 * Used only in Slave mode.
 */
void SPI4_IRQHandler(void)
{
    /*
     * Check RXNE flag:
     * Indicates new received data is available
     */
    if (SPI_REG->SR & (1 << 0))
    {
        /* Read received byte */
        uint8 rxData = SPI_REG->DR;

        /*
         * Call application callback if registered
         */
        if (SlaveRxCallback != NULL)
        {
            SlaveRxCallback(rxData);
        }
    }
}