#pragma once
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
// The legacy SDK predates standard integer types. MSVC's __int8 typedef is
// incompatible with stdint.h's signed char. Give SDK-only typedefs private names
// without changing the supplied headers or any field sizes/ABI layout.
#define int8_t Zbar1cSdkInt8
#define uint8_t Zbar1cSdkUint8
#define int16_t Zbar1cSdkInt16
#define uint16_t Zbar1cSdkUint16
#define int32_t Zbar1cSdkInt32
#define uint32_t Zbar1cSdkUint32
#define int64_t Zbar1cSdkInt64
#define uint64_t Zbar1cSdkUint64
#endif
#include "ComponentBase.h"
#include "IMemoryManager.h"
#include "AddInDefBase.h"
#ifdef _WIN32
#undef int8_t
#undef uint8_t
#undef int16_t
#undef uint16_t
#undef int32_t
#undef uint32_t
#undef int64_t
#undef uint64_t
#endif
