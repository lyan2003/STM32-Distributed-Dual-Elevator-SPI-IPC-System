#ifndef SPI_H
#define SPI_H

#include "Std_Types.h"

/* SPI4 Registers Definition */
typedef struct {
    volatile uint32 CR1;
    volatile uint32 CR2;
    volatile uint32 SR;
    volatile uint32 DR;
    volatile uint32 CRCPR;
    volatile uint32 RXCRCR;
    volatile uint32 TXCRCR;
    volatile uint32 I2SCFGR;
    volatile uint32 I2SPR;
} SpiType;

#define SPI4_BASE_ADDR 0x40013400
#define SPI_REG       ((SpiType*)SPI4_BASE_ADDR)

/* Configuration Macros */
#define SPI_SLAVE  0
#define SPI_MASTER 1

#define SPI_IDLE_LOW  0
#define SPI_IDLE_HIGH 1

#define SPI_SAMPLE_FIRST_TRANSITION  0
#define SPI_SAMPLE_SECOND_TRANSITION 1

/* ==================================================================== */
/* Callbacks Definitions                                                */
/* ==================================================================== */

/* Callback for Slave: Triggered when a single byte is received */
typedef void (*Spi_RxCallback_t)(uint8 rxData);

/* Callback for Master: Triggered when the full buffer transfer is done */
typedef void (*Spi_MasterDoneCallback_t)(void);

/* ==================================================================== */
/* Public API                                                           */
/* ==================================================================== */

void Spi4_Init(uint8 MasterSlave, uint8 ClkPol, uint8 ClkPhase);

/* Master Non-Blocking Transmit/Receive using Interrupts */
void Spi4_MasterTransmitReceive(uint8* txBuf, uint8* rxBuf, uint16 length, Spi_MasterDoneCallback_t callback);
void Spi4_MasterSetCS(uint8 state);

/* Slave API */
void Spi4_SetSlaveCallback(Spi_RxCallback_t callback);
void Spi4_SlavePreloadData(uint8 TxData);


#endif /* SPI_H */