// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/SubsystemAPI.h>
#include <Nodos/Name.hpp>

#include "nosDeviceSubsystem/nosDeviceSubsystem.h"
#include "nosDeviceSubsystem/Device_generated.h"
#include "nosDeviceSubsystem/EditorEvents_generated.h"

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0); // APITransition: Reminder that this should be reset after next major!

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sys::device
{
std::unordered_map<uint32_t, nosDeviceSubsystem*> GExportedSubsystemVersions;

struct DeviceProperties
{
	nosDeviceId Id;
	nos::Name VendorName;
	nos::Name ModelName;
	uint64_t TopologicalId;
	std::string SerialNumber;
	nosDeviceFlags Flags;
	nos::Name DisplayName;

	DeviceProperties() = default;
	DeviceProperties(nosDeviceId id, const nosRegisterDeviceParams& params)
		: Id (id)
		, VendorName(params.Device.VendorName)
		, ModelName(params.Device.ModelName)
		, SerialNumber(params.Device.SerialNumber)
		, Flags(params.Device.Flags)
		, TopologicalId(params.Device.TopologicalId)
		, DisplayName(params.DisplayName)
	{}
};

struct DeviceManager
{
	DeviceManager(const DeviceManager&) = delete;
	DeviceManager& operator=(const DeviceManager&) = delete;
	static DeviceManager& GetInstance() { return Instance; }

	nosResult RegisterDevice(const nosRegisterDeviceParams& params, nosDeviceId* outDeviceId)
	{
		// TODO: Validate
		std::unique_lock lock(DevicesMutex);
		++NextDeviceId;
		DeviceProperties props(NextDeviceId, params);
		*outDeviceId = NextDeviceId;
		Devices[*outDeviceId] = std::move(props);
		SendDeviceListToEditors();
		return NOS_RESULT_SUCCESS;
	}

	nosResult UnregisterDevice(nosDeviceId deviceId)
	{
		std::unique_lock lock(DevicesMutex);
		auto it = Devices.find(deviceId);
		if (it == Devices.end())
			return NOS_RESULT_NOT_FOUND;
		Devices.erase(it);
		SendDeviceListToEditors();
		return NOS_RESULT_SUCCESS;
	}

	nosResult GetSuitableDevice(const nosDeviceInfo& query, nosDeviceId* outDeviceId)
	{
		std::shared_lock lock(DevicesMutex);
		std::unordered_set<DeviceProperties*> suitableDevices;

		// Filter devices by vendor name
		for (auto& [id, props] : Devices)
			if (props.VendorName == query.VendorName)
				if ((query.Flags & props.Flags) == query.Flags) // Queried flags must be supported by the device
					suitableDevices.insert(&props);

		if (suitableDevices.empty())
			return NOS_RESULT_NOT_FOUND;

		for (auto* device : suitableDevices)
		{
			if (device->ModelName == query.ModelName) // First, check model name
			{
				if (device->TopologicalId == query.TopologicalId) // Then, check topological ID
				{
					*outDeviceId = device->Id;
					return NOS_RESULT_SUCCESS;
				}
			}
		}

		// No same model found at the same slot, return the first serial name match.
		for (auto* device : suitableDevices)
		{
			if (device->SerialNumber == query.SerialNumber)
			{
				*outDeviceId = device->Id;
				return NOS_RESULT_SUCCESS;
			}
		}

		// Return the first model name match.
		for (auto* device : suitableDevices)
		{
			if (device->ModelName == query.ModelName)
			{
				*outDeviceId = device->Id;
				return NOS_RESULT_SUCCESS;
			}
		}

		*outDeviceId = (*suitableDevices.begin())->Id;
		return NOS_RESULT_SUCCESS;
	}

private:
	DeviceManager() = default;

	void SendDeviceListToEditors()
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<DeviceInfo>> devices;
		for (auto& [id, props] : Devices)
		{
			auto deviceInfo = CreateDeviceInfoDirect(fbb, props.VendorName.AsCStr(),
				props.ModelName.AsCStr(), props.TopologicalId, props.SerialNumber.c_str(), (DeviceFlags)props.Flags);
			devices.push_back(deviceInfo);
		}
		auto offset = editor::CreateDeviceListDirect(fbb, &devices);
		auto event  = editor::CreateSubsystemEvent(fbb, editor::SubsystemEventUnion::DeviceList, offset.Union());
		fbb.Finish(event);
		nos::Buffer buf = fbb.Release();
		nosEngine.SendCustomMessageToEditors(nosEngine.Module->Id.Name, buf);
	}

	static DeviceManager Instance;

	std::shared_mutex DevicesMutex;
	std::unordered_map<nosDeviceId, DeviceProperties> Devices;
	nosDeviceId NextDeviceId = 0;
};

DeviceManager DeviceManager::Instance{};

nosResult NOSAPI_CALL RegisterDevice(const nosRegisterDeviceParams* params, nosDeviceId* outDeviceId)
{
	if (!params || !outDeviceId)
		return NOS_RESULT_INVALID_ARGUMENT;
	return DeviceManager::GetInstance().RegisterDevice(*params, outDeviceId);
}

nosResult NOSAPI_CALL UnregisterDevice(nosDeviceId deviceId)
{
	return DeviceManager::GetInstance().UnregisterDevice(deviceId);
}

nosResult NOSAPI_CALL GetSuitableDevice(const nosDeviceInfo* info, nosDeviceId* outDeviceId)
{
	if (!info || !outDeviceId)
		return NOS_RESULT_INVALID_ARGUMENT;
	return DeviceManager::GetInstance().GetSuitableDevice(*info, outDeviceId);
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
