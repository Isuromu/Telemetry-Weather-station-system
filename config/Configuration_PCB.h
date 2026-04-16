#pragma once
#include <Arduino.h>

/*
  Configuration_PCB.h

  PCB profile selector.

  The active PCB is selected by the PlatformIO environment build flags.
  Each firmware/example env in platformio.ini should extend one station/PCB
  env that defines exactly one PCB_PROFILE_* macro.
*/

#if defined(PCB_PROFILE_MEGA2560_V1)
  #include "../pcb/PCB_MEGA2560_V1.h"
#elif defined(PCB_PROFILE_ESP32S3_V2)
  #include "../pcb/PCB_ESP32S3_V2.h"
#elif defined(PCB_PROFILE_ESP32S3_V2_TEST)
  #include "../pcb/PCB_ESP32S3_V2_TEST.h"
#else
  #error "No PCB profile selected. Set one PCB_PROFILE_* macro in this PlatformIO environment."
#endif
