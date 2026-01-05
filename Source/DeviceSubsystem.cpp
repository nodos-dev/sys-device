// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginAPI.h>
#include <Nodos/Name.hpp>
#include <Nodos/Plugin.hpp>

#include "nosSysDevice/nosDeviceSubsystem.h"
#include "nosSysDevice/Device_generated.h"
#include "nosSysDevice/EditorEvents_generated.h"

NOS_INIT() 

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sys::device
{
std::unordered_map<uint32_t, nosDeviceSubsystem*> GExportedAPIVersions;

struct DeviceProperties
{
	nosDeviceId Id;

	// Characteristics
	nos::Name OwnerPluginName; // The plugin that registered the device
	nos::Name VendorName;
	nos::Name ModelName;
	uint64_t TopologicalId;
	nos::Name SerialNumber;
	nosDeviceFlags Flags;

	// Other properties
	nos::Name DisplayName;
	uint64_t Handle;

	std::unordered_map<nos::Name, std::string> Properties; // Additional properties

	DeviceProperties() = default;
	DeviceProperties(nosDeviceId id, const nosRegisterDeviceParams& params, nos::Name pluginName)
		: Id (id)
		, VendorName(params.Device.VendorName)
		, ModelName(params.Device.ModelName)
		, SerialNumber(params.Device.SerialNumber)
		, Flags(params.Device.Flags)
		, TopologicalId(params.Device.TopologicalId)
		, DisplayName(params.DisplayName)
		, Handle(params.Handle)
		, OwnerPluginName(pluginName)
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

	nosResult RegisterDevice(const nosRegisterDeviceParams& params, nos::Name callingPluginName, nosDeviceId* outDeviceId)
	{
		// TODO: Validate
		std::unique_lock lock(DevicesMutex);
		++NextDeviceId;
		DeviceProperties props(NextDeviceId, params, callingPluginName);
		for (uint32_t i = 0; i < params.PropertyCount; i++) {
			props.Properties[params.Properties[i].Name] = params.Properties[i].Value;
		}

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
		std::string listName = NOS_DEVICE_SUBSYSTEM_NAME ".DeviceList." + vendorName;
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

	nosResult GetDeviceProperties(nosDeviceId deviceId, nosDeviceProperty* outProperties, uint64_t* outPropertiesCount) {
		std::shared_lock lock(DevicesMutex);
		auto it = Devices.find(deviceId);
		if (it == Devices.end())
			return NOS_RESULT_NOT_FOUND;
		if (outPropertiesCount)
			*outPropertiesCount = it->second.Properties.size();
		if (outProperties) {
			uint32_t i = 0;
			for (auto& [name, val] : it->second.Properties) {
				outProperties[i].Name = nos::Name(name);
				outProperties[i].Value = val.c_str();
			}
		}
		return NOS_RESULT_SUCCESS;
	}

	void SendDeviceListToEditor(uint64_t editorId)
	{
		std::shared_lock lock(DevicesMutex);
		SendDeviceListToEditorsUnlocked(editorId);
	}

private:
	DeviceManager() = default;

	void OnDeviceListUpdated()
	{
		SendDeviceListToEditorsUnlocked();
		UpdateDeviceNamedValuesUnlocked();
	}

	void SendDeviceListToEditorsUnlocked(std::optional<uint64_t> editorId = std::nullopt)
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<DeviceInfo>> devices;
		std::vector<flatbuffers::Offset<DeviceExtraInfo>> deviceExtras;
		for (auto& [id, props] : Devices)
		{
			std::vector<flatbuffers::Offset<DeviceProperty>> properties;
			for (auto& property : props.Properties) {
				properties.push_back(CreateDevicePropertyDirect(fbb, property.first.AsCStr(), property.second.c_str()));
			}
			devices.push_back(CreateDeviceInfoDirect(fbb, props.VendorName.AsCStr(),
				props.ModelName.AsCStr(), props.TopologicalId, props.SerialNumber.AsCStr(), (DeviceFlags)props.Flags));
			deviceExtras.push_back(CreateDeviceExtraInfoDirect(fbb, props.OwnerPluginName.AsCStr(), &properties));
			
		}
		auto offset = editor::CreateDeviceListDirect(fbb, &devices, &deviceExtras);
		auto event  = editor::CreateSubsystemEvent(fbb, editor::SubsystemEventUnion::DeviceList, offset.Union());
		fbb.Finish(event);
		nos::Buffer buf = fbb.Release();
		nosSendEditorMessageParams params {
			.TypeName = NOS_NAME("nos.sys.device.editor.SubsystemEvent"),
			.Message = buf,
			.DispatchType = NOS_EDITOR_MESSAGE_DISPATCH_TYPE_BROADCAST,
		};
		if (editorId)
		{
			params.DispatchType = NOS_EDITOR_MESSAGE_DISPATCH_TYPE_TO_SELECTED;
			params.ToSelected.EditorId = *editorId;
		}
		nosEngine.SendEditorMessage(&params);
	}

	void UpdateDeviceNamedValuesUnlocked()
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
			fb::TNamedValues namedValues;
			namedValues.name = GetDeviceListName(vendor);
			for (auto& device : devices)
			{
				fb::TNamedValue value;
				auto modelNameStr = device.ModelName.AsString();
				value.value_name = modelNameStr + " - " + std::to_string(++modelIndices[vendor][modelNameStr]);
				value.type_name = NOS_DEVICE_SUBSYSTEM_NAME ".DeviceInfo";
				auto buf = device.GetDeviceInfoPinValue();
				value.pin_value = buf;
				namedValues.values.emplace_back(std::make_unique<fb::TNamedValue>(std::move(value)));
			}
			fb::TNamedValue none;
			none.value_name = "None";
			none.type_name = NOS_DEVICE_SUBSYSTEM_NAME ".DeviceInfo";
			none.pin_value = nos::Buffer::From(NoneDeviceInfo());
			namedValues.values.emplace_back(std::make_unique<fb::TNamedValue>(std::move(none)));
			fb::TNamedValue unknown;
			unknown.value_name = "Unknown";
			unknown.type_name = NOS_DEVICE_SUBSYSTEM_NAME ".DeviceInfo";
			namedValues.values.emplace_back(std::make_unique<fb::TNamedValue>(std::move(unknown)));
			update.added_or_updated.emplace_back(std::make_unique<fb::TNamedValues>(std::move(namedValues)));
		}
		std::unordered_set<std::string> newNvNames;
		for (auto& newNv : update.added_or_updated)
			newNvNames.insert(newNv->name);
		for (auto& cur : NamedValueNames)
			if (!newNvNames.contains(cur))
				update.deleted.push_back(cur);
		NamedValueNames = newNvNames;
		SendNamedValueUpdates(update);
	}

	static DeviceManager Instance;

	std::shared_mutex DevicesMutex;
	std::unordered_map<nosDeviceId, DeviceProperties> Devices;
	nosDeviceId NextDeviceId = 0;
	std::unordered_set<std::string> NamedValueNames;
};

DeviceManager DeviceManager::Instance{};

nosResult NOSAPI_CALL RegisterDevice(const nosRegisterDeviceParams* params, nosDeviceId* outDeviceId)
{
	if (!params || !outDeviceId)
		return NOS_RESULT_INVALID_ARGUMENT;

	nosPluginInfo callingPlugin{};
	if (nosEngine.GetCallingPlugin(&callingPlugin) != NOS_RESULT_SUCCESS)
		nosEngine.LogW("RegisterDevice: Failed to get calling plugin info.");

	return DeviceManager::GetInstance().RegisterDevice(*params, callingPlugin.Id.Name, outDeviceId);
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

nosResult NOSAPI_CALL GetDeviceProperties(nosDeviceId deviceId, nosDeviceProperty* outProperties, uint64_t* outPropertiesCount) {
	return DeviceManager::GetInstance().GetDeviceProperties(deviceId, outProperties, outPropertiesCount);
}

nosResult NOSAPI_CALL Export(uint32_t minorVersion, void** outSubsystemContext)
{
	auto it = GExportedAPIVersions.find(minorVersion);
	if (it != GExportedAPIVersions.end())
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
	subsystem->GetDeviceProperties = GetDeviceProperties;
	*outSubsystemContext = subsystem;
	GExportedAPIVersions[minorVersion] = subsystem;
	return NOS_RESULT_SUCCESS;
}

void NOSAPI_CALL OnEditorConnected(uint64_t editorId)
{
	DeviceManager::GetInstance().SendDeviceListToEditor(editorId);
}
	
extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequestAPI = Export;
	subsystemFunctions->OnEditorConnected = OnEditorConnected;
	return NOS_RESULT_SUCCESS;
}
}
}
