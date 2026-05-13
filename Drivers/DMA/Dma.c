#include "Dma.h"
#include "Dma_Private.h"

void Dma_InitStream(const Dma_ConfigType* Config) {
    uint32 controllerAddr = Dma_BaseMap[Config->ControllerId];
    DmaStreamType* dmaStream = GET_STREAM(controllerAddr, Config->StreamNum);

    // 1. Disable Stream and wait until effectively disabled
    dmaStream->CR &= ~(1U << 0);
    while(dmaStream->CR & (1U << 0));

    // 2. Configure Stream
    uint32 configValue = 0;
    configValue |= (uint32)(Config->ChannelNum << 25);
    configValue |= (uint32)(Config->Priority << 16);
    configValue |= (1U << 10); // MINC: Memory Increment
    configValue |= (uint32)(Config->Direction << 6);

    dmaStream->CR = configValue;

    // 3. Set Peripheral Address
    dmaStream->PAR = Config->PeriAddr;
}

void Dma_StartTransfer(uint8 ControllerId, uint8 StreamNum, uint32 MemAddr, uint16 Size) {
    uint32 controllerAddr = Dma_BaseMap[ControllerId];

    DmaBaseType* dmaBase   = (DmaBaseType*)controllerAddr;
    DmaStreamType* dmaStream = GET_STREAM(controllerAddr, StreamNum);

    // 1. Stop to Update
    dmaStream->CR &= ~(1U << 0);
    while(dmaStream->CR & (1U << 0));

    // 2. Clear Flags
    uint32 mask = 0x3D;
    if (StreamNum <= 3) {
        uint8 shift = (StreamNum <= 1) ? (StreamNum * 6) : (StreamNum * 6 + 4);
        dmaBase->LIFCR = (mask << shift);
    } else {
        uint8 offset = StreamNum - 4;
        uint8 shift = (offset <= 1) ? (offset * 6) : (offset * 6 + 4);
        dmaBase->HIFCR = (mask << shift);
    }

    // 3. Set Memory Address & Size
    dmaStream->M0AR = MemAddr;
    dmaStream->NDTR = Size;

    // 4. Start!
    dmaStream->CR |= (1U << 0);
}

uint8 Dma_IsTransferComplete(uint8 ControllerId, uint8 StreamNum) {
    DmaBaseType* dmaBase = (DmaBaseType*)Dma_BaseMap[ControllerId];
    uint32 status;
    uint8 shift;

    if (StreamNum <= 3) {
        status = dmaBase->LISR;
        shift = (StreamNum <= 1) ? (StreamNum * 6 + 5) : (StreamNum * 6 + 9);
    } else {
        status = dmaBase->HISR;
        uint8 offset = StreamNum - 4;
        shift = (offset <= 1) ? (offset * 6 + 5) : (offset * 6 + 9);
    }

    return (status & (1U << shift)) ? 1 : 0;
}