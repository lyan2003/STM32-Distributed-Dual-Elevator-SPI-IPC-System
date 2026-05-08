#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include "../Lib/Std_Types.h"

#define RB_SIZE 256   /* should be a power of two for fast modulo */

typedef struct
{
    uint8 buffer[RB_SIZE];
    volatile uint32 writeIndex;
    volatile uint32 readIndex;
    volatile uint32 count;// how much data is available
} RingBufferType;

void RB_Init(RingBufferType* rb);
boolean RB_Enqueue(RingBufferType* rb, uint8 byte); // returns false if full (byte dropped) write
boolean RB_Dequeue(RingBufferType* rb, uint8* byte); // returns false if empty   read
uint8 RB_Peek(const RingBufferType* rb);
boolean RB_IsEmpty(const RingBufferType* rb);
boolean RB_IsFull(const RingBufferType* rb);
uint32 RB_Available(const RingBufferType* rb);
void RB_Flush(RingBufferType* rb);

#endif //RINGBUFFER_H
