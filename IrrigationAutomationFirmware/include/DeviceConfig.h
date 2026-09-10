#pragma once

#include <ProjectConfig.h>

#if ACTIVE_INVERTER == INVERTER_DELIXI_E100G2R2T4B
#include <config/inverters/Delixi_E100G2R2T4B.h>
namespace irrigation {
inline constexpr const InverterProfile &ActiveInverter =
    inverters::Delixi_E100G2R2T4B;
}
#else
#error "Unknown ACTIVE_INVERTER selection"
#endif

#if ACTIVE_MOTOR == MOTOR_GRANDFAR_2CP50_160B
#include <config/motors/Grandfar_2CP50_160B.h>
namespace irrigation {
inline constexpr const MotorProfile &ActiveMotor =
    motors::Grandfar_2CP50_160B;
}
#else
#error "Unknown ACTIVE_MOTOR selection"
#endif

