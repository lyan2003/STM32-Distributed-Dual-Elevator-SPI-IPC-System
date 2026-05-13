#ifndef DMA_PRIVATE_H
#define DMA_PRIVATE_H

#include "Std_Types.h"

/* 1. خريطة ريجسترات الـ Stream (دي اللي هتكرر 8 مرات) */
typedef struct {
    volatile uint32 CR;
    volatile uint32 NDTR;
    volatile uint32 PAR;
    volatile uint32 M0AR;
    volatile uint32 M1AR;
    volatile uint32 FCR;
} DmaStreamType;

/* 2. خريطة ريجسترات الـ Base (المسؤولة عن الـ Interrupts والـ Flags) */
typedef struct {
    volatile uint32 LISR;
    volatile uint32 HISR;
    volatile uint32 LIFCR;
    volatile uint32 HIFCR;
} DmaBaseType;

#define DMA1_BASE_ADDR    0x40026000UL
#define DMA2_BASE_ADDR    0x40026400UL

/* مصفوفة العناوين */
static const uint32 Dma_BaseMap[] = {DMA1_BASE_ADDR, DMA2_BASE_ADDR};

/* الماكرو ده بيحسب عنوان الـ Stream ويحوله لبوينتر لـ Struct */
#define GET_STREAM(CONTROLLER_ADDR, STREAM_IDX) \
((DmaStreamType*)((CONTROLLER_ADDR) + 0x10 + ((STREAM_IDX) * 0x18)))

#endif // DMA_PRIVATE_H