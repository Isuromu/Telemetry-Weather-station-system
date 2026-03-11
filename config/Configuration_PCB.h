#pragma once
#include <Arduino.h>
#include "Configuration.h"

/*
  Configuration_PCB.h

  PCB profile selector.

  This file chooses which PCB description file is active
  for the current build.
*/

#if defined(PCB_PROFILE_ESP32S3_V2_TEST)
  #include "../pcb/PCB_ESP32S3_V2_TEST.h"
#else
  #error "No PCB profile selected in Configuration.h"
#endif