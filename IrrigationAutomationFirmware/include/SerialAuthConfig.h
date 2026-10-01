#pragma once

#if __has_include("SerialAuthSecrets.h")
#include "SerialAuthSecrets.h"
#else
#include "SerialAuthSecrets.example.h"
#endif

// Field builds keep Serial diagnostics and local commands locked until the
// password is entered. PlatformIO build flags may override any setting here.
#ifndef ENABLE_SERIAL_AUTH_GATE
#define ENABLE_SERIAL_AUTH_GATE 1
#endif

// This matches the serial-debug switch used by the Amudario firmware. It is
// used only when the auth gate is deliberately disabled for a bench build.
#ifndef SERIAL_DEBUG_DEFAULT
#define SERIAL_DEBUG_DEFAULT 0
#endif

#ifndef SERIAL_AUTH_MAX_INPUT_LENGTH
#define SERIAL_AUTH_MAX_INPUT_LENGTH 64
#endif

// The no-button design accepts a password only during this non-blocking boot
// window. Set it to 0 to keep the window open for a controlled bench build.
#ifndef SERIAL_AUTH_TIMEOUT_MS
#define SERIAL_AUTH_TIMEOUT_MS 30000UL
#endif
