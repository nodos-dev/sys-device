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

typedef struct nosDeviceProperty
{
	nosName Name;
	const char* Value;
} nosDeviceProperty;

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
	nosDeviceProperty* Properties;
	uint64_t PropertyCount; // Number of properties in the Properties array
} nosRegisterDeviceParams;

typedef struct nosDeviceSubsystem {
	nosResult (NOSAPI_CALL* RegisterDevice)(const nosRegisterDeviceParams* params, nosDeviceId* outDeviceId);
	nosResult (NOSAPI_CALL* UnregisterDevice)(nosDeviceId deviceId);
	nosResult (NOSAPI_CALL* GetSuitableDevice)(const nosDeviceInfo* info, nosDeviceId* outDeviceId);
	nosResult (NOSAPI_CALL* GetDeviceListNameForVendor)(nosName vendorName, nosName* outName);
	nosResult (NOSAPI_CALL* GetDeviceHandle)(nosDeviceId deviceId, uint64_t* outHandle);
	nosResult (NOSAPI_CALL* GetDeviceInfo)(nosDeviceId deviceId, nosDeviceInfo* outInfo);
	void (NOSAPI_CALL* GetDevicesWithVendor)(nosName vendorName, nosDeviceId* outDevices, uint64_t* outCount);
	nosResult (NOSAPI_CALL* GetDeviceProperties)(nosDeviceId deviceId, nosDeviceProperty* outProperties, uint64_t* outPropertiesCount);
} nosDeviceSubsystem;

#pragma region Helper Declarations & Macros

// Make sure these are same with nossys file.
#define NOS_DEVICE_SUBSYSTEM_NAME "nos.sys.device"
#define NOS_DEVICE_SUBSYSTEM_VERSION_MAJOR 1
#define NOS_DEVICE_SUBSYSTEM_VERSION_MINOR 0

extern struct nosPluginInfo nosDeviceSubsystemModuleInfo;
extern nosDeviceSubsystem* nosDevice;

#define NOS_DEVICE_SUBSYSTEM_INIT()         	\
	nosPluginInfo nosDeviceSubsystemModuleInfo; \
	nosDeviceSubsystem* nosDevice = nullptr;

#define NOS_DEVICE_SUBSYSTEM_IMPORT() NOS_IMPORT_DEP(NOS_DEVICE_SUBSYSTEM_NAME, nosDeviceSubsystemModuleInfo, nosDevice)

#pragma endregion

#if __cplusplus
}
#endif

#pragma region C++ Helpers
#if __cplusplus
#include "Device_generated.h"
namespace nos::sys::device
{

inline nosDeviceInfo ConvertDeviceInfo(TDeviceInfo const& info)
{
	return {
		.VendorName = nos::Name(info.vendor_name),
		.ModelName = nos::Name(info.model_name),
		.TopologicalId = info.topological_id,
		.SerialNumber = nos::Name(info.serial_number),
		.Flags = static_cast<nosDeviceFlags>(info.flags)
	};
}

inline TDeviceInfo ConvertDeviceInfo(nosDeviceInfo const& info)
{
	TDeviceInfo dev{};
	dev.vendor_name = nos::Name(info.VendorName).AsString();
	dev.model_name = nos::Name(info.ModelName).AsString();
	dev.topological_id = info.TopologicalId;
	dev.serial_number = nos::Name(info.SerialNumber).AsString();
	dev.flags = static_cast<nos::sys::device::DeviceFlags>(info.Flags);
	return dev;
}

inline TDeviceInfo NoneDeviceInfo()
{
	TDeviceInfo dev{};
	dev.vendor_name = "None";
	return dev;
}

inline std::string GetDeviceListNameForVendor(nos::Name vendorName)
{
	nosName outName{};
	auto res = nosDevice->GetDeviceListNameForVendor(vendorName, &outName);
	assert(res == NOS_RESULT_SUCCESS);
	return nos::Name(outName).AsString();
}

inline std::vector<sys::device::TDeviceInfo> GetDevicesWithVendor(nos::Name vendorName)
{
	uint64_t count{};
	nosDevice->GetDevicesWithVendor(vendorName, nullptr, &count);
	std::vector<nosDeviceId> devices(count);
	nosDevice->GetDevicesWithVendor(vendorName, devices.data(), &count);
	std::vector<sys::device::TDeviceInfo> deviceInfos;
	for (auto& device : devices)
	{
		nosDeviceInfo info{};
		nosDevice->GetDeviceInfo(device, &info);
		deviceInfos.push_back(sys::device::ConvertDeviceInfo(info));
	}
	return deviceInfos;
}

}
#endif
#pragma endregion

#endif