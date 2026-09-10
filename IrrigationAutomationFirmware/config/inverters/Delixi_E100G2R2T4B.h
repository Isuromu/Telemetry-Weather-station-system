#pragma once

#include <ConfigTypes.h>

namespace irrigation::inverters {

inline constexpr InverterProfile Delixi_E100G2R2T4B{
    "Delixi_E100G2R2T4B",
    "DELIXI",
    "CDI-E100",
    "CDI-E100G2R2T4B",
    2.2f,
    6.0f,
    380,
    1,
    9600,
    SerialFrame::EightN1,
    2,
    300,
    3,
    50.0f,
    50.0f,
    20.0f,
    20.0f,
    0.0f  // Initial bring-up only; enable a fail-safe timeout for deployment.
};

}  // namespace irrigation::inverters
