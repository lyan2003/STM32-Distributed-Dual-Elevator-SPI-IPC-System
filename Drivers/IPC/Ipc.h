#ifndef IPC_H
#define IPC_H

#include "Std_Types.h"
#include "ElevatorB.h"

#define IPC_HEADER_BYTE 0xA5
#define IPC_PACKET_SIZE 8

typedef struct {
    uint8 Header;
    uint8 State;
    uint8 CurrentFloor;
    uint8 TargetFloor;
    uint8 Emergency;
    uint8 Reserved1;
    uint8 Reserved2;
    uint8 Checksum;
} IpcPacket_t;

/* Global TX Packet (Defined in main.c, used by MasterLogic.c to set targets) */
extern IpcPacket_t MasterTxPacket;

/* ==========================================
 * Slave API (Elevator B)
 * ========================================== */
void Ipc_SlaveInit(void);
void Ipc_SlaveUpdateState(ElevatorContextB_t ctx);

/* ==========================================
 * Master API (Elevator A / Main Controller)
 * ========================================== */
void Ipc_MasterInit(void);
void Ipc_MasterRun(IpcPacket_t* txPacket);
uint8 Ipc_MasterLinkOk(void);
IpcPacket_t Ipc_MasterGetRxPacket(void);

#endif /* IPC_H */