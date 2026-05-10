#ifndef MASTERLOGIC_H
#define MASTERLOGIC_H

#include "Std_Types.h"

/* ========================================== */
/* Request Direction Macros                   */
/* ========================================== */

/* Elevator request directions */
#define DIR_UP     1
#define DIR_DOWN  -1

/* ========================================== */
/* Initialization Functions                   */
/* ========================================== */

/**
 * @brief Initialize master dispatch logic.
 *
 * Used to reset internal queues and states.
 */
void MasterLogic_Init(void);

/* ========================================== */
/* Runtime Update Function                    */
/* ========================================== */

/**
 * @brief Periodic update function.
 *
 * Should be called inside the main loop
 * to dispatch pending requests whenever
 * an elevator becomes idle.
 */
void MasterLogic_Update(void);

/* ========================================== */
/* Dispatch Algorithm                         */
/* ========================================== */

/**
 * @brief Main dispatch algorithm.
 *
 * Applies the scheduling rules to determine
 * the best elevator for the request.
 *
 * @param call_floor Requested floor.
 * @param call_dir   Request direction.
 */
void MasterLogic_DispatchCall(uint8 call_floor,
                              sint8 call_dir);

/* ========================================== */
/* Hallway Button Callbacks                   */
/* ========================================== */

/**
 * @brief Floor 1 Up button callback.
 */
void MasterLogic_F1_Up_Callback(void);

/**
 * @brief Floor 2 Up button callback.
 */
void MasterLogic_F2_Up_Callback(void);

/**
 * @brief Floor 2 Down button callback.
 */
void MasterLogic_F2_Down_Callback(void);

/**
 * @brief Floor 3 Up button callback.
 */
void MasterLogic_F3_Up_Callback(void);

/**
 * @brief Floor 3 Down button callback.
 */
void MasterLogic_F3_Down_Callback(void);

/**
 * @brief Floor 4 Down button callback.
 */
void MasterLogic_F4_Down_Callback(void);

#endif /* MASTERLOGIC_H */