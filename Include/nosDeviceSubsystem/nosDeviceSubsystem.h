/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#ifndef NOS_SYS_DEVICE_H_INCLUDED
#define NOS_SYS_DEVICE_H_INCLUDED

#if __cplusplus
extern "C"
{
#endif

#include <Nodos/Types.h>

typedef uint64_t nosDeviceHandle;

typedef struct nosRegisterDeviceParams {
	nosName UniqueVendorName;
	nosName DeviceModelName;
	const char* DeviceSerialNumber;
} nosRegisterDeviceParams;

typedef struct nosDeviceSubsystem {
	nosResult (NOSAPI_CALL* RegisterDevice)(const nosRegisterDeviceParams* params, nosDeviceHandle* outDeviceHandle);
	nosResult (NOSAPI_CALL* UnregisterDevice)(nosDeviceHandle deviceHandle);
	// TODO
} nosDeviceSubsystem;

#pragma region Helper Declarations & Macros

// Make sure these are same with nossys file.
#define NOS_SYS_DEVICE_SUBSYSTEM_NAME "nos.sys.device"
#define NOS_SYS_DEVICE_SUBSYSTEM_VERSION_MAJOR 0
#define NOS_SYS_DEVICE_SUBSYSTEM_VERSION_MINOR 1

extern struct nosModuleInfo nosDeviceSubsystemModuleInfo;
extern nosDeviceSubsystem* nosDevice;

#define NOS_SYS_DEVICE_SUBSYSTEM_INIT()         \
	nosModuleInfo nosDeviceSubsystemModuleInfo; \
	nosDeviceSubsystem* nosDevice = nullptr;

#define NOS_SYS_DEVICE_SUBSYSTEM_IMPORT() NOS_IMPORT_DEP(NOS_SYS_DEVICE_SUBSYSTEM_NAME, nosDeviceSubsystemModuleInfo, nosDevice)

#pragma endregion

#if __cplusplus
}
#endif

#endif