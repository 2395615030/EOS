// EmotionOS Compiler Compatibility Header
// Included automatically for MSVC/Clang-cl builds
#pragma once

// MSVC needs explicit _USE_MATH_DEFINES for math constants
#ifdef _MSC_VER
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#define NOMINMAX
#endif

// Ensure windows.h doesn't define min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
