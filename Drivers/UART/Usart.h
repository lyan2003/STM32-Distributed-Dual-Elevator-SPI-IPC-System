#ifndef USART_H
#define USART_H

#include "Std_Types.h"

#define Tx_OK    0U
#define Tx_NOK   1U

void Usart1_Init(void);
uint8 Usart1_TransmitByte(uint8 Byte);
uint8 Usart1_ReceiveByte(void);

void Usart1_TransmitString(const char* Str);
void Usart1_TransmitStringDMA(const char* Str);

// Ring Buffer
uint8 Usart1_GetByte(uint8 *data);
char* Usart1_ReadLine(char *buffer, uint32 maxLen);

#endif /* USART_H */