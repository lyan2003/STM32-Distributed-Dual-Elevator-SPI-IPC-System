#include "elevator.h"

int main(void) {
    ElevatorA_Init();

    while(1) {
        ElevatorA_RunFSM();
    }
}