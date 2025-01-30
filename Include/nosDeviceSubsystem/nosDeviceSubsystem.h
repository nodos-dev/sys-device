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

typedef uint64_t nosDeviceId;

#define NOS_DEVICE_FLAG_NONE 0
#define NOS_DEVICE_FLAG_PCIE (1ull << 0)
#define NOS_DEVICE_FLAG_GRAPHICS (1ull << 1)
#define NOS_DEVICE_FLAG_VIDEO_IO (1ull << 2)

typedef struct nosDeviceInfo
{
	nosName VendorName;
	nosName ModelName;
	const char* SerialNumber;
	uint64_t Flags;
	int32_t PCIeAddress; // If flags contain PCIE, this field will be read.
} nosDeviceInfo;

typedef struct nosRegisterDeviceParams {
	nosDeviceInfo Device;
	nosName DisplayName;
	uint64_t Handle; // Handle used by module to access to the device
} nosRegisterDeviceParams;
	
// 1. Modules register their devices
// 2. Associate the resulting device id with thier device objects
// 3. 

typedef struct nosDeviceSubsystem {
	nosResult (NOSAPI_CALL* RegisterDevice)(const nosRegisterDeviceParams* params, nosDeviceId* outDeviceId);
	nosResult (NOSAPI_CALL* UnregisterDevice)(nosDeviceId deviceId);
	nosResult (NOSAPI_CALL* GetSuitableDevice)(const nosDeviceInfo* info, nosDeviceId* outDeviceId);
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