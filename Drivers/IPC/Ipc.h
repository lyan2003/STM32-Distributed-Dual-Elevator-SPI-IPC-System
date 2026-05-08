#ifndef IPC_H
#define IPC_H

#include "Std_Types.h"
#include "ElevatorB.h"

#define IPC_HEADER_BYTE 0xA5
#define IPC_PACKET_SIZE 8

typedef struct {
    uint8 Header;       // 0xA5
    uint8 State;        // IDLE, MOVING_UP, etc.
    uint8 CurrentFloor; // 1 -> 4
    uint8 TargetFloor;  // 1 -> 4
    uint8 Emergency;    // 0 or 1
    uint8 Reserved1;    // 0
    uint8 Reserved2;    // 0
    uint8 Checksum;     // Sum of Byte 0 to Byte 6
} IpcPacket_t;

/* (Elevator B) */
void Ipc_SlaveInit(void);
void Ipc_SlaveUpdateState(ElevatorContextB_t ctx);

/* (Elevator A / Main Controller) */
void Ipc_MasterInit(void);
uint8 Ipc_MasterExchangeState(IpcPacket_t* txPacket, IpcPacket_t* rxPacket);

#endif /* IPC_H */