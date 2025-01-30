// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/SubsystemAPI.h>
#include <Nodos/Name.hpp>

#include "nosDeviceSubsystem/nosDeviceSubsystem.h"

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0); // APITransition: Reminder that this should be reset after next major!

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sys::device
{
std::unordered_map<uint32_t, nosDeviceSubsystem*> GExportedSubsystemVersions;

struct DeviceProperties
{
	nos::Name VendorName;
	nos::Name ModelName;
	std::string SerialNumber;
	uint64_t Flags;
	int32_t PCIeAddress;
	nos::Name DisplayName;

	DeviceProperties(const nosRegisterDeviceParams& params)
		: VendorName(params.Device.VendorName)
		, ModelName(params.Device.ModelName)
		, SerialNumber(params.Device.SerialNumber)
		, Flags(params.Device.Flags)
		, PCIeAddress(params.Device.PCIeAddress)
		, DisplayName(params.DisplayName)
	{}
};

struct DeviceManager
{
	DeviceManager(const DeviceManager&) = delete;
	DeviceManager& operator=(const DeviceManager&) = delete;
	static DeviceManager& GetInstance() { return Instance; }

	nosResult RegisterDevice(const nosRegisterDeviceParams* params, nosDeviceId* outDeviceId)
	{
		if (!params || !outDeviceId)
			return NOS_RESULT_INVALID_ARGUMENT;
		// TODO: Validate
		std::unique_lock lock(DevicesMutex);
		++NextDeviceId;
		DeviceProperties props(*params);
		*outDeviceId = NextDeviceId;
		Devices[*outDeviceId] = std::move(props);
		return NOS_RESULT_SUCCESS;
	}

	nosResult UnregisterDevice(nosDeviceId deviceId)
	{
		std::unique_lock lock(DevicesMutex);
		auto it = Devices.find(deviceId);
		if (it == Devices.end())
			return NOS_RESULT_NOT_FOUND;
		Devices.erase(it);
		return NOS_RESULT_SUCCESS;
	}
	
private:
	DeviceManager() = default;
	static DeviceManager Instance;

	std::shared_mutex DevicesMutex;
	std::unordered_map<nosDeviceId, DeviceProperties> Devices;
	nosDeviceId NextDeviceId = 0;
};

DeviceManager DeviceManager::Instance{};

nosResult NOSAPI_CALL RegisterDevice(const nosRegisterDeviceParams* params, nosDeviceId* outDeviceId)
{
	return DeviceManager::GetInstance().RegisterDevice(params, outDeviceId);
}

nosResult NOSAPI_CALL UnregisterDevice(nosDeviceId deviceId)
{
	return DeviceManager::GetInstance().UnregisterDevice(deviceId);
}
	
nosResult NOSAPI_CALL Export(uint32_t minorVersion, void** outSubsystemContext)
{
	auto it = GExportedSubsystemVersions.find(minorVersion);
	if (it != GExportedSubsystemVersions.end())
	{
		*outSubsystemContext = it->second;
		return NOS_RESULT_SUCCESS;
	}
	auto* subsystem = new nosDeviceSubsystem();
	subsystem->RegisterDevice = RegisterDevice;
	subsystem->UnregisterDevice = UnregisterDevice;
	*outSubsystemContext = subsystem;
	GExportedSubsystemVersions[minorVersion] = subsystem;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL Initialize()
{
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL UnloadSubsystem()
{
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportSubsystem(nosSubsystemFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequest = Export;
	subsystemFunctions->Initialize = Initialize;
	subsystemFunctions->OnPreUnloadSubsystem = UnloadSubsystem;
	return NOS_RESULT_SUCCESS;
}
}
}
