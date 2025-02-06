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

typedef enum nosDeviceFlags {
	NOS_DEVICE_FLAG_NONE = 0,
	NOS_DEVICE_FLAG_PCI = (1 << 0),
	NOS_DEVICE_FLAG_GRAPHICS = (1 << 1),
	NOS_DEVICE_FLAG_VIDEO_IO = (1 << 2),
} nosDeviceFlags;

typedef struct nosDeviceInfo
{
	nosName VendorName;
	nosName ModelName;
	uint64_t TopologicalId;
	nosName SerialNumber;
	nosDeviceFlags Flags;
} nosDeviceInfo;

typedef struct nosRegisterDeviceParams {
	nosDeviceInfo Device;
	nosName DisplayName;
	uint64_t Handle; // Handle used by module to access to the device
} nosRegisterDeviceParams;

typedef struct nosDeviceSubsystem {
	nosResult (NOSAPI_CALL* RegisterDevice)(const nosRegisterDeviceParams* params, nosDeviceId* outDeviceId);
	nosResult (NOSAPI_CALL* UnregisterDevice)(nosDeviceId deviceId);
	nosResult (NOSAPI_CALL* GetSuitableDevice)(const nosDeviceInfo* info, nosDeviceId* outDeviceId);
	nosResult (NOSAPI_CALL* GetDeviceListNameForVendor)(nosName vendorName, nosName* outName);
	nosResult (NOSAPI_CALL* GetDeviceHandle)(nosDeviceId deviceId, uint64_t* outHandle);
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

#pragma region C++ Helpers
#if __cplusplus
#include "Device_generated.h"
namespace nos::sys::device
{
inline nosDeviceInfo ConvertDeviceInfo(nos::sys::device::DeviceInfo const& info)
{
	return {
		.VendorName = nos::Name(info.vendor_name() ? info.vendor_name()->string_view() : ""),
		.ModelName = nos::Name(info.model_name() ? info.model_name()->string_view() : ""),
		.TopologicalId = info.topological_id(),
		.SerialNumber = nos::Name(info.serial_number() ? info.serial_number()->string_view() : ""),
		.Flags = static_cast<nosDeviceFlags>(info.flags()),
	};
}

inline nos::sys::device::TDeviceInfo ConvertDeviceInfo(nosDeviceInfo const& info)
{
	return {
		.vendor_name = nos::Name(info.VendorName).AsString(),
		.model_name = nos::Name(info.ModelName).AsString(),
		.topological_id = info.TopologicalId,
		.serial_number = nos::Name(info.SerialNumber).AsString(),
		.flags = static_cast<nos::sys::device::DeviceFlags>(info.Flags),
	};
}

inline nos::sys::device::TDeviceInfo NoneDeviceInfo()
{
	return {.vendor_name = "None"};
}
}
#endif
#pragma endregion

#endif