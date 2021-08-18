/**
 ******************************************************************************
 * @file    mc_config.h
 * @author  Motor Control SDK Team, ST Microelectronics
 * @brief   Motor Control Subsystem components configuration and handler
 *          structures declarations.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */

#ifndef __MC_CONFIG_H
#define __MC_CONFIG_H

#include "circle_limitation.h"
#include "hall_speed_pos_fdbk.h"
#include "ntc_temperature_sensor.h"
#include "pid_regulator.h"
#include "pqd_motor_power_measurement.h"
#include "pwm_curr_fdbk.h"
#include "r3_2_f30x_pwm_curr_fdbk.h"
#include "r_divider_bus_voltage_sensor.h"
#include "ramp_ext_mngr.h"
#include "revup_ctrl.h"
#include "speed_torq_ctrl.h"
#include "virtual_bus_voltage_sensor.h"
#include "virtual_speed_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
extern PID_Handle_t PIDSpeedHandle_M1;
extern PID_Handle_t PIDIqHandle_M1;
extern PID_Handle_t PIDIdHandle_M1;
extern NTC_Handle_t TempSensorParamsM1;
extern PWMC_R3_2_Handle_t PWM_Handle_M1;
extern SpeednTorqCtrl_Handle_t SpeednTorqCtrlM1;
extern PQD_MotorPowMeas_Handle_t PQD_MotorPowMeasM1;
extern PQD_MotorPowMeas_Handle_t *pPQD_MotorPowMeasM1;
extern HALL_Handle_t HALL_M1;
extern RDivider_Handle_t RealBusVoltageSensorParamsM1;
extern CircleLimitation_Handle_t CircleLimitationM1;
extern RampExtMngr_Handle_t RampExtMngrHFParamsM1;

#define NBR_OF_MOTORS 1
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* __MC_CONFIG_H */
/******************* (C) COPYRIGHT 2019 STMicroelectronics *****END OF FILE****/
