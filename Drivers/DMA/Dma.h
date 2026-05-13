#ifndef DMA_H
#define DMA_H

#include "Std_Types.h"

/* DMA Controllers Indices */
#define DMA_ID_1    0
#define DMA_ID_2    1

/* Directions */
#define DMA_PERI_TO_MEM    0x00
#define DMA_MEM_TO_PERI    0x01
#define DMA_MEM_TO_MEM     0x02

/* Configuration Struct */
typedef struct {
    uint8  ControllerId;  /* DMA_ID_1 or DMA_ID_2 */
    uint8  StreamNum;     /* 0 to 7 */
    uint8  ChannelNum;    /* 0 to 7 */
    uint8  Direction;     /* MEM_TO_PERI, etc. */
    uint32 PeriAddr;      /* Peripheral address */
    uint8  Priority;      /* 0:Low to 3:Very High */
} Dma_ConfigType;

/* Function Prototypes */
void Dma_InitStream(const Dma_ConfigType* Config);
void Dma_StartTransfer(uint8 ControllerId, uint8 StreamNum, uint32 MemAddr, uint16 Size);
uint8 Dma_IsTransferComplete(uint8 ControllerId, uint8 StreamNum);

#endif // DMA_H