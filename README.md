# Distributed Dual-Elevator Control System over Full-Duplex SPI IPC

A production-grade, distributed reactive software architecture implemented entirely from scratch at the register level on dual STM32 ARM Cortex-M4 microcontrollers. This repository showcases an advanced safety-critical multi-processor system utilizing a custom full-duplex Inter-Processor Communication (IPC) protocol over SPI, atomic critical section protection, and a master-driven deterministic dispatcher algorithm designed to optimize multi-floor traffic scheduling with zero CPU polling overhead.

### Project Demonstration and Simulation Walkthrough
The firmware exhibits real-time asynchronous event routing, hardware-level packet serialization, distributed finite state synchronization, and high-priority NVIC preemption under emergency failure modes:
* **[Watch System Walkthrough and Simulation Video]()**

---

## Distributed Hardware Architecture

The system topology divides workload and orchestration responsibilities across two physically decoupled microcontrollers executing concurrently:

```text
+------------------------------------+          +------------------------------------+
|         MASTER MCU (BOARD A)       |          |         SLAVE MCU (BOARD B)        |
|  [Elevator A FSM] + [Dispatcher]   |          |          [Elevator B FSM]          |
|                                    |          |                                    |
|  - 4 x Cabin Request EXTI Lines   |          |  - 4 x Cabin Request EXTI Lines   |
|  - 4 x Floor Sensor EXTI Lines     |          |  - 4 x Floor Sensor EXTI Lines     |
|  - 6 x Hallway Call EXTI Lines     |          |                                    |
|  - PWM Motor Drive + UART DMA      |          |  - PWM Motor Drive                 |
+------------------------------------+          +------------------------------------+
                  |                                               |
                  +----------------- [ SPI IPC BUS ] -------------+
                         (SCK, MOSI, MISO, CS @ 50ms Cycles)

```

### 1. Input/Output Peripheral Mapping

* **Asynchronous Event Handlers (MCAL EXTI/NVIC):** All physical input vectors operate strictly through External Interrupts (`EXTI`) to enforce a non-blocking reactive design. Polling for input hardware state changes is strictly prohibited.
* **Emergency Stop Channels:** Bound to the absolute highest configurable NVIC priority group. Pressing the button preempts all active computational state loops instantly, halting the actuators and forcing the localized node into an isolated fallback mode.
* **Position & Call Request Transducers:** Floor position indicators, internal cabin requests, and building hallway calls are captured instantly via discrete interrupt lines, updating state memory variables deterministically.


* **Actuation and Communication Pipelines:**
* **PWM Motor Phase Simulation:** Implements custom general-purpose hardware timers configured in PWM mode. Speed ramping profiles translate dynamically into register duty cycles ($0\%$ Stop, $20\%$ Slow approach, $100\%$ Full-speed transit).
* **DMA-Accelerated Telemetry Subsystem:** A non-blocking telemetry engine streams system health matrices over a UART-to-PC interface every 500ms. By offloading data byte manipulation to a Direct Memory Access (DMA) channel, the MCU core sustains zero performance penalties for serial string formatting and debugging.



---

## Inter-Processor Communication (SPI IPC Protocol)

Data alignment and distributed state machine synchronization are achieved via a specialized full-duplex SPI network ticking at 50ms periods.

### 1. The Slave Pre-loading Architecture

To comply with strict non-blocking hardware constraints, the Slave peripheral firmware bypasses traditional delayed interrupt evaluation by pre-loading its internal status frames directly into the SPI Transmission (`TX`) hardware data register *prior* to the Master asserting the Chip Select (CS) hardware line. This ensures instantaneous symmetric byte swapping upon clock initiation.

### 2. 8-Byte Serialization Frame Format

Every communication packet is tightly packed into a rigid, non-padded struct layout to maintain byte alignment neutrality:

```text
+---------------+---------------+---------------+-------------------+---------------+
|  Byte 0       |  Byte 1       |  Byte 2       |  Byte 3 -> 6      |  Byte 7       |
+---------------+---------------+---------------+-------------------+---------------+
| Header (0xA5) | Node State    | Current Floor | Telemetry Data    | CRC Checksum  |
+---------------+---------------+---------------+-------------------+---------------+

```

* **Error Detection:** Every frame terminates with a generated arithmetic checksum block. If a corrupted frame violates the packet layout validation check, the data payload is safely dropped, mitigating glitched motion vectors.

---

## Task Allocation and Dispatcher Optimization Algorithm

The Master MCU acts as the centralized system dispatcher. Upon receiving an external Hallway Floor Call event, the core parses the states of both elevators through an explicit directional priority array to choose the optimal actuator path.

| Operational Scenario | Structural Decision Rule |
| --- | --- |
| **Communication Fault** | Upon detecting an SPI timeout, the Master claims all hallway requests. The Slave detaches cleanly into an isolated, independent emergency loop. |
| **Immediate Match** | If an elevator is already stationary at the requested target floor in an `IDLE` state, it claims the target call instantly. |
| **Perfect Match** | If an elevator is already in transit towards the call vector in an identical directional trajectory, it is assigned the call (e.g., Car at Floor 1 moving UP; Hall Call at Floor 2 seeking UP). |
| **Passed Match** | If an elevator is moving in the requested direction but has already physically bypassed the target floor layout, it is reassigned to a lower priority index. |
| **Opposite Direction** | If an elevator is moving completely away from the call direction target, it is isolated from assignment bounds until its active movement path terminates cleanly. |
| **Idle Vector** | If no active directional trajectories align with the call, the dispatcher calculates absolute floor distances and commands the nearest stationary `IDLE` car. |

---

## System Demonstration

This section showcases the actual execution output of the integrated distributed system.

The following video demonstrates the complete hardware simulation, including coordinated multi-node task allocation, full-duplex real-time packet exchange over SPI, high-priority asynchronous emergency preemption, and active PWM motor profile transitions:

---

## Advanced Software Engineering Frameworks Applied

* **Race Condition Mitigation:** Every status flag and data variable shared between asynchronous Interrupt Service Routines (`ISR`) and the primary Finite State Machine loop is explicitly declared with the `volatile` type qualifier to eliminate aggressive compiler register-caching optimization anomalies.
* **Atomic Critical Section Isolation:** Buffer operations on the primary SPI RX/TX queues are wrapped inside atomic encapsulation wrappers (`Enter_Critical()` / `Exit_Critical()`). This manipulates prime priority register configurations to isolate execution streams during read-modify-write tasks, ensuring absolute data integrity.
* **Deterministic Concurrency:** Bypasses software polling loops completely. All timing operations, debounce timeouts, and state delay triggers are driven directly via internal hardware base timer configurations and automated callback events.

---

## Repository Directory Tree

```text
project4-distributed-elevator-ipc/
├── Master_Node/              # Master Microcontroller Subsystem Software
│   ├── App/                  # Dispatcher Scheduling Algorithm & Elevator A FSM
│   ├── Drivers/              # MCAL: Full-Duplex SPI, EXTI, Timer PWM, UART DMA
│   └── src/                  # Master Translation Units & Main Execution
├── Slave_Node/               # Slave Microcontroller Subsystem Software
│   ├── App/                  # Elevator B Local Finite State Machine Logic
│   ├── Drivers/              # MCAL: Non-Blocking Register SPI, EXTI, Timer PWM
│   └── src/                  # Slave Translation Units & Main Execution
├── Shared_Lib/               # Common Type Primitives and Shared Packet Definitions
├── Lab_Preparation/          # Register Memory Maps, PWM Mathematics & Frame Topologies
├── Simulation_Schematics/    # Multi-MCU Proteus Hardware Emulation Circuits
└── cmake/                    # Distributed Toolchain Compilation Directives

```

---

## Toolchain Setup and Deployment

### Prerequisites

* Cross-Compiler Toolchain: `arm-none-eabi-gcc`.
* Build Automation Environment: CMake (version 3.15 or higher).
* Hardware Simulation Target: Proteus Design Suite (configured for dual-Cortex-M4 operations).

### Build Pipeline

1. Clone the repository including the distributed node configurations:
```bash
git clone git@github.com:lyan2003/STM32-Distributed-Dual-Elevator-SPI-IPC-System.git

```


2. **Compile Master Node Binary:**
```bash
cd Master_Node
cmake -B build && cmake --build build

```


3. **Compile Slave Node Binary:**
```bash
cd ../Slave_Node
cmake -B build && cmake --build build

```


4. Flash the respective compiled target `.hex` or `.bin` assets directly into the designated Master and Slave simulation IC blocks.
