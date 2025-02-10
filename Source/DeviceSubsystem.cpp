// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/SubsystemAPI.h>
#include <Nodos/Name.hpp>
#include <Nodos/Helpers.hpp>

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

	// Characteristics
	nos::Name VendorName;
	nos::Name ModelName;
	uint64_t TopologicalId;
	nos::Name SerialNumber;
	nosDeviceFlags Flags;

	// Other properties
	nos::Name DisplayName;
	uint64_t Handle;

	DeviceProperties() = default;
	DeviceProperties(nosDeviceId id, const nosRegisterDeviceParams& params)
		: Id (id)
		, VendorName(params.Device.VendorName)
		, ModelName(params.Device.ModelName)
		, SerialNumber(params.Device.SerialNumber)
		, Flags(params.Device.Flags)
		, TopologicalId(params.Device.TopologicalId)
		, DisplayName(params.DisplayName)
		, Handle(params.Handle)
	{}

	nos::Table<DeviceInfo> GetDeviceInfoPinValue() const
	{
		TDeviceInfo info;
		info.vendor_name = VendorName.AsString();
		info.model_name = ModelName.AsString();
		info.topological_id = TopologicalId;
		info.serial_number = SerialNumber.AsString();
		info.flags = (DeviceFlags)Flags;
		return nos::Buffer::From(info);
	}
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
		OnDeviceListUpdated();
		return NOS_RESULT_SUCCESS;
	}

	nosResult UnregisterDevice(nosDeviceId deviceId)
	{
		std::unique_lock lock(DevicesMutex);
		auto it = Devices.find(deviceId);
		if (it == Devices.end())
			return NOS_RESULT_NOT_FOUND;
		Devices.erase(it);
		OnDeviceListUpdated();
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

	std::string GetDeviceListName(const std::string& vendorName)
	{
		std::string listName = NOS_SYS_DEVICE_SUBSYSTEM_NAME ".DeviceList." + vendorName;
		return listName;
	}

	nosResult GetDeviceHandle(nosDeviceId id, uint64_t* outHandle)
	{
		std::shared_lock lock(DevicesMutex);
		auto it = Devices.find(id);
		if (it == Devices.end())
			return NOS_RESULT_NOT_FOUND;
		if (outHandle)
			*outHandle = it->second.Handle;
		return NOS_RESULT_SUCCESS;
	}

	nosResult GetDeviceInfo(nosDeviceId id, nosDeviceInfo* outInfo)
	{
		std::shared_lock lock(DevicesMutex);
		auto it = Devices.find(id);
		if (it == Devices.end())
			return NOS_RESULT_NOT_FOUND;
		if (outInfo)
		{
			outInfo->VendorName = it->second.VendorName;
			outInfo->ModelName = it->second.ModelName;
			outInfo->TopologicalId = it->second.TopologicalId;
			outInfo->SerialNumber = it->second.SerialNumber;
			outInfo->Flags = it->second.Flags;
		}
		return NOS_RESULT_SUCCESS;
	}

	void GetDevicesWithVendor(nosName vendorName, nosDeviceId* outDevices, uint64_t* outCount)
	{
		std::shared_lock lock(DevicesMutex);
		std::vector<nosDeviceId> devices;
		for (auto& [id, props] : Devices)
			if (props.VendorName == vendorName)
				devices.push_back(id);
		if (outCount)
			*outCount = devices.size();
		if (outDevices)
			std::copy(devices.begin(), devices.end(), outDevices);
	}

private:
	DeviceManager() = default;

	void OnDeviceListUpdated()
	{
		SendDeviceListToEditors();
		UpdateDeviceNamedValues();
	}

	void SendDeviceListToEditors()
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<DeviceInfo>> devices;
		for (auto& [id, props] : Devices)
		{
			auto deviceInfo = CreateDeviceInfoDirect(fbb, props.VendorName.AsCStr(),
				props.ModelName.AsCStr(), props.TopologicalId, props.SerialNumber.AsCStr(), (DeviceFlags)props.Flags);
			devices.push_back(deviceInfo);
		}
		auto offset = editor::CreateDeviceListDirect(fbb, &devices);
		auto event  = editor::CreateSubsystemEvent(fbb, editor::SubsystemEventUnion::DeviceList, offset.Union());
		fbb.Finish(event);
		nos::Buffer buf = fbb.Release();
		nosSendEditorMessageParams params { .Message = buf, .DispatchType = NOS_SEND_MESSAGE_TO_EDITOR_TYPE_BROADCAST};
		nosEngine.SendEditorMessage(&params);
	}

	void UpdateDeviceNamedValues()
	{
		TUpdateNamedValues update;
		std::unordered_map<std::string, std::vector<DeviceProperties>> map;
		for (auto& [id, props] : Devices)
		{
			map[props.VendorName.AsString()].push_back(props);
		}
		std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> modelIndices;
		for (auto& [vendor, devices] : map)
		{
			for (auto& device : devices)
			{
				modelIndices[vendor][device.ModelName.AsString()]++;
			}
		}
		for (auto& [vendor, devices] : map)
		{
			fb::TNamedValues namedValues;
			namedValues.name = GetDeviceListName(vendor);
			for (auto& device : devices)
			{
				fb::TNamedValue value;
				auto modelNameStr = device.ModelName.AsString();
				value.value_name = modelNameStr + " - " + std::to_string(modelIndices[vendor][modelNameStr]);
				value.type_name = NOS_SYS_DEVICE_SUBSYSTEM_NAME ".DeviceInfo";
				auto buf = device.GetDeviceInfoPinValue();
				value.pin_value = buf;
				namedValues.values.emplace_back(std::make_unique<fb::TNamedValue>(std::move(value)));
			}
			fb::TNamedValue none;
			none.value_name = "None";
			none.type_name = NOS_SYS_DEVICE_SUBSYSTEM_NAME ".DeviceInfo";
			none.pin_value = nos::Buffer::From(NoneDeviceInfo());
			namedValues.values.emplace_back(std::make_unique<fb::TNamedValue>(std::move(none)));
			fb::TNamedValue unknown;
			unknown.value_name = "Unknown";
			unknown.type_name = NOS_SYS_DEVICE_SUBSYSTEM_NAME ".DeviceInfo";
			namedValues.values.emplace_back(std::make_unique<fb::TNamedValue>(std::move(unknown)));
			update.updated_values.emplace_back(std::make_unique<fb::TNamedValues>(std::move(namedValues)));
		}
		SendNamedValueUpdates(update);
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

nosResult NOSAPI_CALL GetDeviceListName(nosName vendorName, nosName* outNamedValueListName)
{
	if (!outNamedValueListName)
		return NOS_RESULT_INVALID_ARGUMENT;
	*outNamedValueListName = nos::Name(DeviceManager::GetInstance().GetDeviceListName(nos::Name(vendorName).AsString()));
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL GetDeviceHandle(nosDeviceId deviceId, uint64_t* outHandle)
{
	return DeviceManager::GetInstance().GetDeviceHandle(deviceId, outHandle);
}

nosResult NOSAPI_CALL GetDeviceInfo(nosDeviceId deviceId, nosDeviceInfo* outInfo)
{
	return DeviceManager::GetInstance().GetDeviceInfo(deviceId, outInfo);
}

void NOSAPI_CALL GetDevicesWithVendor(nosName vendorName, nosDeviceId* outDevices, uint64_t* outCount)
{
	DeviceManager::GetInstance().GetDevicesWithVendor(nos::Name(vendorName), outDevices, outCount);
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
	subsystem->GetSuitableDevice = GetSuitableDevice;
	subsystem->GetDeviceListNameForVendor = GetDeviceListName;
	subsystem->GetDeviceHandle = GetDeviceHandle;
	subsystem->GetDeviceInfo = GetDeviceInfo;
	subsystem->GetDevicesWithVendor = GetDevicesWithVendor;
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
