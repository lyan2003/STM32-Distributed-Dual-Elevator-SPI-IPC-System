#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "Std_Types.h"

/* * Call once during system init.
 * Configures USART pins and starts the 500ms non-blocking ticker.
 * Pass the runtime role (1 for Master, 0 for Slave).
 */
void Telemetry_Init(uint8 isMasterFlag);

/* * Call every iteration of the main loop.
 * Only transmits when the 500ms flag is set.
 */
void Telemetry_Run(uint8 isMasterFlag);

#endif /* TELEMETRY_H */