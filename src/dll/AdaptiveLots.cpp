#include "AdaptiveLots.h"

#include "LotMapping.h"
#include "Patcher.h"
#include "PropertyUtil.h"
#include "RingMetadata.h"
#include "utils/Logger.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <windows.h>

#include "GZServPtrs.h"
#include "cGZPersistResourceKey.h"
#include "cIGZPersistResourceKeyList.h"
#include "cIGZPersistResourceManager.h"
#include "cIGZVariant.h"
#include "cIGZString.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4LotConfigurationManager.h"
#include "cISC4NetworkManager.h"
#include "cISC4NetworkOccupant.h"
#include "cISC4NetworkTool.h"
#include "cISCProperty.h"
#include "cISCPropertyHolder.h"
#include "cISCResExemplar.h"
#include "cISCResExemplarCohort.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "SCPropertyUtil.h"
#include "SC4UI.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
	static_assert(sizeof(void*) == 4, "SC4AdaptiveLotsDll requires a 32-bit build");

	constexpr uintptr_t kPlaceLotConstructor = 0x004C0430;
	constexpr uintptr_t kConstructorCall = 0x007EA49B;
	constexpr uintptr_t kAutomaticFacing = 0x004BEB70;
	constexpr uintptr_t kAutomaticFacingCall = 0x004C345E;
	constexpr uintptr_t kPlaceLotMouseMove = 0x004C3300;
	constexpr uintptr_t kChangeLotConfiguration = 0x004C1360;
	constexpr uintptr_t kPlaceLot = 0x004C1740;
	constexpr uintptr_t kPlaceLotVtable = 0x00A90738;
	constexpr uintptr_t kPlaceLotOnKeyDown = 0x004C3060;
	constexpr size_t kOnKeyDownVtableSlot = 14;
	constexpr size_t kOnMouseMoveVtableSlot = 20;
	constexpr uintptr_t kTollPicker = 0x004BFB30;
	constexpr uintptr_t kTollPreviewCall = 0x004C3415;
	constexpr uintptr_t kTollCommitCall = 0x004C186C;
	constexpr uintptr_t kCatalogBuildingAccepted = 0x007F0194;
	constexpr uintptr_t kCatalogBuildingAcceptedContinue = 0x007F019A;
	constexpr uintptr_t kCatalogBuildingRejected = 0x007F065B;
	constexpr size_t kLotConfigIdOffset = 0x30;
	constexpr size_t kTollBoothFlagOffset = 0x150;
	constexpr uint32_t kCursorTextId = 0xE9C877FA;
	constexpr uint32_t kPropertyExemplarType = 0x00000010;
	constexpr uint32_t kExemplarTypeBuilding = 0x00000002;
	constexpr uint32_t kExemplarTypeLotConfiguration = 0x00000010;
	constexpr uint32_t kLotObjectPropertyStart = 0x88EDC900;
	constexpr uint32_t kLotObjectPropertyEnd = 0x88EDCDFF;
	constexpr size_t kLotObjectTypeIndex = 0;
	constexpr size_t kLotObjectBuildingIdIndex = 12;

	struct NetworkCellInfo
	{
		uint32_t x;
		uint32_t z;
		uint32_t networkTypeFlags;
		cISC4NetworkOccupant* networkOccupant;
	};
	static_assert(offsetof(NetworkCellInfo, networkOccupant) == 0x0C);

	class NetworkTool
	{
	public:
		NetworkCellInfo* GetCellInfo(const uint32_t xz)
		{
			using Function = NetworkCellInfo* (__thiscall*)(NetworkTool*, uint32_t);
			return reinterpret_cast<Function>(0x00633220)(this, xz);
		}
	};

	LotMapping::Table mappings;
	void* activeControl;
	uint32_t originalQueryId;
	uint32_t selectedLotConfigId;
	uint32_t selectedVariant = 0;
	uint32_t selectedNetwork = UINT32_MAX;
	uint32_t selectedPiece = UINT32_MAX;
	RingMetadata::LotInfoMap lotInfoByConfigId;
	RingMetadata::BuildingSet hiddenBuildingExemplarIds;
	std::unordered_map<uint32_t, std::string> ringNameByQueryId;
	bool catalogMenuFilteringInstalled = false;
	using KeyDown = bool (__thiscall*)(void*, int32_t, uint32_t);
	using MouseMove = bool (__thiscall*)(void*, int32_t, int32_t, uint32_t);
	using ChangeLotConfiguration = void (__thiscall*)(void*, uint32_t);
	KeyDown originalKeyDown = nullptr;
	MouseMove originalMouseMove = nullptr;
	using ExemplarKeyIndex = std::unordered_map<uint32_t, std::vector<cGZPersistResourceKey>>;
	cISC4City* GetCity();

	bool LoadExemplarFromKeyByType(
		cIGZPersistResourceManager* const resourceManager,
		const cGZPersistResourceKey& key,
		const uint32_t expectedExemplarType,
		cRZAutoRefCount<cISCResExemplar>& exemplar)
	{
		cRZAutoRefCount<cISCResExemplar> candidate;
		if (!resourceManager
			|| !resourceManager->GetResource(
				key, GZIID_cISCResExemplar, candidate.AsPPVoid(), 0, nullptr)
			|| !candidate)
		{
			return false;
		}

		const cISCPropertyHolder* const properties = candidate->AsISCPropertyHolder();
		uint32_t exemplarType = 0;
		if (!properties
			|| !SCPropertyUtil::GetPropertyValue(properties, kPropertyExemplarType, exemplarType)
			|| exemplarType != expectedExemplarType)
		{
			return false;
		}

		exemplar = candidate;
		return true;
	}

	bool LoadExemplarByType(
		cIGZPersistResourceManager* const resourceManager,
		const ExemplarKeyIndex& keysByInstance,
		const uint32_t instanceId,
		const uint32_t expectedExemplarType,
		cRZAutoRefCount<cISCResExemplar>& exemplar,
		cGZPersistResourceKey* const resolvedKey = nullptr)
	{
		const auto keys = keysByInstance.find(instanceId);
		if (!resourceManager || keys == keysByInstance.end())
		{
			return false;
		}

		for (const cGZPersistResourceKey& key : keys->second)
		{
			if (LoadExemplarFromKeyByType(
				resourceManager, key, expectedExemplarType, exemplar))
			{
				if (resolvedKey)
				{
					*resolvedKey = key;
				}
				return true;
			}
		}
		return false;
	}

	std::string GetDisplayName(const cISCPropertyHolder* const properties)
	{
		cRZBaseString name;
		return properties && PropertyUtil::GetDisplayName(properties, name)
			? std::string(name.Data())
			: std::string{};
	}

	std::string GetExemplarName(const cISCPropertyHolder* const properties)
	{
		cRZBaseString name;
		return properties && PropertyUtil::GetExemplarName(properties, name)
			? std::string(name.Data())
			: std::string{};
	}

	RingMetadata::LotInfo ResolveLotInfo(
		cIGZPersistResourceManager* const resourceManager,
		const ExemplarKeyIndex& keysByInstance,
		const uint32_t lotConfigId)
	{
		RingMetadata::LotInfo info;
		cRZAutoRefCount<cISCResExemplar> lotExemplar;
		cGZPersistResourceKey lotExemplarKey;
		if (!LoadExemplarByType(
			resourceManager,
			keysByInstance,
			lotConfigId,
			kExemplarTypeLotConfiguration,
			lotExemplar,
			&lotExemplarKey))
		{
			return info;
		}

		const cISCPropertyHolder* const lotProperties = lotExemplar->AsISCPropertyHolder();
		info.displayName = GetDisplayName(lotProperties);
		info.lotExemplarName = GetExemplarName(lotProperties);
		if (!lotProperties)
		{
			return info;
		}

		for (uint32_t propertyId = kLotObjectPropertyStart;
			propertyId <= kLotObjectPropertyEnd;
			++propertyId)
		{
			const cISCProperty* const property = lotProperties->GetProperty(propertyId);
			const cIGZVariant* const value = property ? property->GetPropertyValue() : nullptr;
			if (!value
				|| value->GetType() != cIGZVariant::Uint32Array
				|| value->GetCount() <= kLotObjectBuildingIdIndex)
			{
				continue;
			}

			const uint32_t* const data = value->RefUint32();
			if (!data || data[kLotObjectTypeIndex] != 0 || data[kLotObjectBuildingIdIndex] == 0)
			{
				continue;
			}

			cRZAutoRefCount<cISCResExemplar> buildingExemplar;
			const uint32_t buildingExemplarId = data[kLotObjectBuildingIdIndex];
			const cGZPersistResourceKey sameGroupKey(
				LotMapping::kExemplarType, lotExemplarKey.group, buildingExemplarId);
			if (!LoadExemplarFromKeyByType(
					resourceManager, sameGroupKey, kExemplarTypeBuilding, buildingExemplar)
				&& !LoadExemplarByType(
					resourceManager,
					keysByInstance,
					buildingExemplarId,
					kExemplarTypeBuilding,
					buildingExemplar))
			{
				// Rep 13 can contain a building family; unresolved references are not hidden.
				continue;
			}

			info.buildingExemplarId = buildingExemplarId;
			if (const std::string buildingName = GetDisplayName(buildingExemplar->AsISCPropertyHolder());
				!buildingName.empty())
			{
				info.buildingDisplayName = buildingName;
				info.displayName = buildingName;
			}
			break;
		}
		return info;
	}

	void RebuildLotInfoCache(cIGZPersistResourceManager* const resourceManager)
	{
		lotInfoByConfigId.clear();
		hiddenBuildingExemplarIds.clear();
		if (!resourceManager)
		{
			return;
		}

		const std::vector<uint32_t> rootQueryIds = mappings.QueryExemplarIds();
		const std::vector<uint32_t> replacementLotConfigIds = mappings.ReplacementLotConfigIds();
		std::unordered_set<uint32_t> workset(rootQueryIds.begin(), rootQueryIds.end());
		workset.insert(replacementLotConfigIds.begin(), replacementLotConfigIds.end());

		cRZAutoRefCount<cIGZPersistResourceKeyList> keys;
		resourceManager->GetAvailableResourceListForType(keys.AsPPObj(), LotMapping::kExemplarType);
		ExemplarKeyIndex keysByInstance;
		if (keys)
		{
			for (uint32_t i = 0; i < keys->Size(); ++i)
			{
				const cGZPersistResourceKey& key = keys->GetKey(i);
				keysByInstance[key.instance].push_back(key);
			}
		}

		lotInfoByConfigId.reserve(workset.size());
		for (const uint32_t lotConfigId : workset)
		{
			lotInfoByConfigId.emplace(
				lotConfigId, ResolveLotInfo(resourceManager, keysByInstance, lotConfigId));
		}
		hiddenBuildingExemplarIds = RingMetadata::BuildHiddenBuildingSet(
			rootQueryIds, replacementLotConfigIds, lotInfoByConfigId);
		if (catalogMenuFilteringInstalled)
		{
			for (const uint32_t queryId : rootQueryIds)
			{
				const std::vector<LotMapping::Entry>* const entries = mappings.Get(queryId);
				if (!entries)
				{
					continue;
				}

				std::unordered_set<uint32_t> loggedLotConfigIds;
				for (const LotMapping::Entry& entry : *entries)
				{
					if (!loggedLotConfigIds.insert(entry.lotConfigId).second)
					{
						continue;
					}

					const auto info = lotInfoByConfigId.find(entry.lotConfigId);
					if (info == lotInfoByConfigId.end()
						|| !hiddenBuildingExemplarIds.contains(info->second.buildingExemplarId))
					{
						continue;
					}

					const auto ringName = ringNameByQueryId.find(queryId);
					LOG_DEBUG(
						"Hiding building '{}' ({:08X}) for lot exemplar '{}' ({:08X}) in ring '{}' ({:08X}).",
						info->second.buildingDisplayName.empty()
							? "<unnamed>" : info->second.buildingDisplayName,
						info->second.buildingExemplarId,
						info->second.lotExemplarName.empty()
							? "<unnamed>" : info->second.lotExemplarName,
						entry.lotConfigId,
						ringName == ringNameByQueryId.end() || ringName->second.empty()
							? "<unnamed>" : ringName->second,
						queryId);
				}
			}
		}
		LOG_INFO(
			"Cached metadata for {} adaptive lot configuration(s); {} replacement building menu item(s) are eligible for hiding.",
			lotInfoByConfigId.size(),
			hiddenBuildingExemplarIds.size());
	}

	void RemoveUnavailableLotConfigurations()
	{
		cISC4City* const city = GetCity();
		cISC4LotConfigurationManager* const lotManager = city
			? city->GetLotConfigurationManager() : nullptr;
		if (!lotManager)
		{
			LOG_WARN(
				"Adaptive lot alternatives could not be validated because the lot configuration manager is unavailable.");
			return;
		}

		const std::vector<LotMapping::RemovedEntry> removed =
			mappings.RemoveUnavailableLotConfigs([lotManager](const uint32_t lotConfigId) {
				return lotManager->GetLotConfiguration(lotConfigId) != nullptr;
			});
		std::unordered_map<uint32_t, std::unordered_set<uint32_t>> missingIdsByCohort;
		std::unordered_set<uint32_t> affectedCohortIds;
		for (const LotMapping::RemovedEntry& item : removed)
		{
			affectedCohortIds.insert(item.queryExemplarId);
			if (!missingIdsByCohort[item.queryExemplarId].insert(item.entry.lotConfigId).second)
			{
				continue;
			}
			LOG_DEBUG(
				"Discarded unavailable lot configuration {:08X} from adaptive mapping cohort {:08X}.",
				item.entry.lotConfigId,
				item.queryExemplarId);
		}
		if (!removed.empty())
		{
			const size_t retainedCount = mappings.EntryCount();
			LOG_INFO(
				"Validated {} adaptive lot mapping entries: retained {}, discarded {} unavailable entries across {} affected cohort(s).",
				retainedCount + removed.size(),
				retainedCount,
				removed.size(),
				affectedCohortIds.size());
		}

		std::erase_if(ringNameByQueryId, [](const auto& item) {
			return !mappings.Contains(item.first);
		});
	}

	std::string GetLotDisplayName(const uint32_t lotConfigId)
	{
		const auto it = lotInfoByConfigId.find(lotConfigId);
		return it == lotInfoByConfigId.end() ? std::string{} : it->second.displayName;
	}

	void UpdateCursorText(const uint32_t lotConfigId, const uint32_t variant, const uint32_t count)
	{
		cRZAutoRefCount<cISC4View3DWin> view = SC4UI::GetView3DWin();
		if (!view) return;
		const std::string name = GetLotDisplayName(lotConfigId);
		cRZBaseString title(name.empty() ? "Adaptive lot" : name);
		if (count > 1)
		{
			title.Append(" (", 2);
			const std::string index = std::to_string(variant + 1);
			title.Append(index.c_str(), static_cast<uint32_t>(index.size()));
			title.Append("/", 1);
			const std::string total = std::to_string(count);
			title.Append(total.c_str(), static_cast<uint32_t>(total.size()));
			title.Append(")", 1);
		}
		view->SetCursorText(kCursorTextId, 0, nullptr, &title, 0);
	}

	void ClearCursorText()
	{
		cRZAutoRefCount<cISC4View3DWin> view = SC4UI::GetView3DWin();
		if (view) view->ClearCursorText(kCursorTextId);
	}
	uint32_t lastLoggedQuery = UINT32_MAX;
	uint32_t lastLoggedNetwork = UINT32_MAX;
	uint32_t lastLoggedPiece = UINT32_MAX;

	template <typename T>
	T& FieldAt(void* object, const size_t offset)
	{
		return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
	}

	cISC4City* GetCity()
	{
		cISC4AppPtr app;
		return app ? app->GetCity() : nullptr;
	}

	void* GetTrafficNetworkInfo(
		void* trafficNetworkMap,
		const int32_t x,
		const int32_t z,
		const uint32_t networkMask)
	{
		using Function = void* (__thiscall*)(void*, int32_t, int32_t, uint32_t, bool);
		void** const vtable = trafficNetworkMap ? *reinterpret_cast<void***>(trafficNetworkMap) : nullptr;
		return vtable && vtable[8]
			? reinterpret_cast<Function>(vtable[8])(trafficNetworkMap, x, z, networkMask, true)
			: nullptr;
	}

	uint32_t FindMappedLotConfig(void* control, const float* cursorPosition)
	{
		if (control != activeControl || !cursorPosition || !mappings.Contains(originalQueryId))
		{
			return 0;
		}

		cISC4City* const city = GetCity();
		if (!city)
		{
			return 0;
		}

		int x = 0;
		int z = 0;
		city->PositionToCell(cursorPosition[0], cursorPosition[2], x, z);
		if (!city->CellIsInBounds(x, z))
		{
			return 0;
		}

		void* const trafficMap = reinterpret_cast<void*>(city->GetTrafficNetwork());
		cISC4NetworkManager* const manager = city->GetNetworkManager();
		cISC4LotConfigurationManager* const lotManager = city->GetLotConfigurationManager();
		const auto* entries = mappings.Get(originalQueryId);
		if (!entries || !lotManager)
		{
			return 0;
		}
		const auto useWildcard = [&]() {
			const uint32_t lotConfigId = mappings.FindWildcard(originalQueryId, selectedVariant);
			return lotConfigId && lotManager->GetLotConfiguration(lotConfigId) ? lotConfigId : 0u;
		};
		if (!trafficMap || !manager)
		{
			selectedNetwork = LotMapping::kWildcardNetworkType;
			selectedPiece = 0;
			return useWildcard();
		}

		std::array<bool, 13> mappedNetworks{};
		for (const auto& entry : *entries)
		{
			if (entry.networkType != LotMapping::kWildcardNetworkType)
			{
				mappedNetworks[entry.networkType] = true;
			}
		}
		for (uint32_t networkType = 0; networkType < mappedNetworks.size(); ++networkType)
		{
			if (!mappedNetworks[networkType])
			{
				continue;
			}

			if (!GetTrafficNetworkInfo(trafficMap, x, z, 1u << networkType))
			{
				continue;
			}

			cISC4NetworkTool* tool = manager->GetNetworkTool(static_cast<int32_t>(networkType), true);
			if (!tool)
			{
				tool = manager->GetNetworkTool(static_cast<int32_t>(networkType), false);
			}
			if (!tool)
			{
				continue;
			}

			tool->Init();
			NetworkCellInfo* const cell = reinterpret_cast<NetworkTool*>(tool)->GetCellInfo(
				(static_cast<uint32_t>(z) << 16) | static_cast<uint16_t>(x));
			cISC4NetworkOccupant* const occupant = cell ? cell->networkOccupant : nullptr;
			if (!occupant || !occupant->IsOfType(static_cast<cISC4NetworkOccupant::eNetworkType>(networkType)))
			{
				continue;
			}
			if (lastLoggedQuery != originalQueryId
				|| lastLoggedNetwork != networkType
				|| lastLoggedPiece != occupant->PieceId())
			{
				lastLoggedQuery = originalQueryId;
				lastLoggedNetwork = networkType;
				lastLoggedPiece = occupant->PieceId();
				LOG_DEBUG(
					"Adaptive query {:08X}: network {}, piece {:08X}, rotation {}, flip {}.",
					originalQueryId,
					networkType,
					lastLoggedPiece,
					occupant->GetRotation(),
					occupant->GetFlip());
			}

			const uint32_t pieceId = occupant->PieceId();
			const uint32_t variantCount = mappings.VariantCount(originalQueryId, networkType, pieceId);
			if (!variantCount)
			{
				continue;
			}
			if (selectedNetwork != networkType || selectedPiece != pieceId)
			{
				selectedNetwork = networkType;
				selectedPiece = pieceId;
				selectedVariant = 0;
			}
			const uint32_t lotConfigId = mappings.Find(
				originalQueryId, networkType, pieceId, selectedVariant);
			if (!lotConfigId)
			{
				return 0;
			}

			if (lotManager->GetLotConfiguration(lotConfigId))
			{
				return lotConfigId;
			}
			return 0;
		}

		selectedNetwork = LotMapping::kWildcardNetworkType;
		selectedPiece = 0;
		return useWildcard();
	}

	using Constructor = void* (__thiscall*)(void*, uint32_t, uint32_t, uint32_t, uint8_t);
	void* __fastcall HookedConstructor(
		void* control,
		void*,
		const uint32_t unknown,
		const uint32_t queryExemplarId,
		const uint32_t flagA,
		const uint8_t flagB)
	{
		void* const result = reinterpret_cast<Constructor>(kPlaceLotConstructor)(
			control, unknown, queryExemplarId, flagA, flagB);
		activeControl = result;
		originalQueryId = queryExemplarId;
		selectedLotConfigId = 0;
		selectedVariant = 0;
		selectedNetwork = UINT32_MAX;
		selectedPiece = UINT32_MAX;
		ClearCursorText();
		LOG_DEBUG("PlaceLot control {:p} created for query {:08X}.", result, queryExemplarId);
		return result;
	}

	bool __fastcall HookedKeyDown(void* control, void*, const int32_t vkCode, const uint32_t modifiers)
	{
		if (control == activeControl && vkCode == VK_ESCAPE)
		{
			selectedLotConfigId = 0;
			ClearCursorText();
		}
		if (control == activeControl && vkCode == VK_TAB)
		{
			const bool wildcard = selectedNetwork == LotMapping::kWildcardNetworkType
				|| (selectedNetwork == UINT32_MAX && selectedPiece == UINT32_MAX);
			const uint32_t count = wildcard
				? mappings.WildcardVariantCount(originalQueryId)
				: mappings.VariantCount(originalQueryId, selectedNetwork, selectedPiece);
			if (count > 1)
			{
				const bool backwards = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
				selectedVariant = backwards
					? (selectedVariant + count - 1) % count
					: (selectedVariant + 1) % count;
				LOG_DEBUG("Adaptive query {:08X}: selected variant {} of {}.",
					originalQueryId, selectedVariant, count);
				const uint32_t lotConfigId = wildcard
					? mappings.FindWildcard(originalQueryId, selectedVariant)
					: mappings.Find(originalQueryId, selectedNetwork, selectedPiece, selectedVariant);
				if (lotConfigId)
				{
					reinterpret_cast<ChangeLotConfiguration>(kChangeLotConfiguration)(control, lotConfigId);
				}
				reinterpret_cast<MouseMove>(kPlaceLotMouseMove)(
					control, FieldAt<int32_t>(control, 0x44), FieldAt<int32_t>(control, 0x48), 0x100);
				if (lotConfigId)
				{
					UpdateCursorText(lotConfigId, selectedVariant, count);
				}
				return true;
			}
		}
		return originalKeyDown ? originalKeyDown(control, vkCode, modifiers) : false;
	}

	bool __fastcall HookedMouseMove(void* control, void*, const int32_t x, const int32_t z, const uint32_t modifiers)
	{
		const bool result = originalMouseMove ? originalMouseMove(control, x, z, modifiers) : false;
		if (control == activeControl && mappings.Contains(originalQueryId))
		{
			if (selectedLotConfigId)
			{
				const uint32_t count = selectedNetwork == LotMapping::kWildcardNetworkType
					? mappings.WildcardVariantCount(originalQueryId)
					: mappings.VariantCount(originalQueryId, selectedNetwork, selectedPiece);
				UpdateCursorText(selectedLotConfigId, selectedVariant, count);
			}
			else
			{
				ClearCursorText();
			}
		}
		return result;
	}

	using AutomaticFacing = bool (__thiscall*)(void*, void*, float*, uint32_t*, void*);
	bool __fastcall HookedAutomaticFacing(
		void* control,
		void*,
		void* lotConfiguration,
		float* cursorPosition,
		uint32_t* orientation,
		void* cellCoverage)
	{
		if (control == activeControl && mappings.Contains(originalQueryId))
		{
			selectedLotConfigId = FindMappedLotConfig(control, cursorPosition);
			const uint32_t lotConfigId = selectedLotConfigId ? selectedLotConfigId : originalQueryId;
			if (FieldAt<uint32_t>(control, kLotConfigIdOffset) != lotConfigId)
			{
				reinterpret_cast<ChangeLotConfiguration>(kChangeLotConfiguration)(control, lotConfigId);
			}
			cISC4City* const city = GetCity();
			if (city && city->GetLotConfigurationManager())
			{
				lotConfiguration = city->GetLotConfigurationManager()->GetLotConfiguration(lotConfigId);
			}
		}

		return reinterpret_cast<AutomaticFacing>(kAutomaticFacing)(
			control, lotConfiguration, cursorPosition, orientation, cellCoverage);
	}

	using TollPicker = bool (__thiscall*)(void*, uint32_t*, uint32_t*, float*, void*);
	bool __fastcall HookedTollPicker(
		void* control,
		void*,
		uint32_t* lotConfigId,
		uint32_t* orientation,
		float* cursorPosition,
		void* cellCoverage)
	{
		const bool result = reinterpret_cast<TollPicker>(kTollPicker)(
			control, lotConfigId, orientation, cursorPosition, cellCoverage);
		if (result)
		{
			selectedLotConfigId = FindMappedLotConfig(control, cursorPosition);
			if (selectedLotConfigId)
			{
				*lotConfigId = selectedLotConfigId;
			}
		}
		return result;
	}

	void __fastcall PreparePlaceLot(void* control, void*)
	{
		if (control == activeControl
			&& !FieldAt<uint8_t>(control, kTollBoothFlagOffset)
			&& mappings.Contains(originalQueryId))
		{
			FieldAt<uint32_t>(control, kLotConfigIdOffset) =
				selectedLotConfigId ? selectedLotConfigId : originalQueryId;
		}
	}

	void NAKED_FUN HookedPlaceLot()
	{
		__asm
		{
			pushfd
			pushad
			call PreparePlaceLot
			popad
			popfd
			sub esp, 0A4h
			push 004C1746h
			ret
		}
	}

	bool IsCallTo(const uintptr_t address, const uintptr_t destination)
	{
		const auto* bytes = reinterpret_cast<const uint8_t*>(address);
		int32_t displacement = 0;
		std::memcpy(&displacement, bytes + 1, sizeof(displacement));
		return bytes[0] == 0xE8 && address + 5 + displacement == destination;
	}

	bool HasPlaceLotPrologue()
	{
		constexpr std::array<uint8_t, 6> expected{0x81, 0xEC, 0xA4, 0x00, 0x00, 0x00};
		return std::memcmp(reinterpret_cast<const void*>(kPlaceLot), expected.data(), expected.size()) == 0;
	}

	bool __fastcall ShouldHideCatalogBuilding(const uint32_t buildingExemplarId)
	{
		return hiddenBuildingExemplarIds.contains(buildingExemplarId);
	}

	void NAKED_FUN HookedCatalogBuildingAccepted()
	{
		__asm
		{
			pushfd
			pushad
			mov eax, dword ptr [esp + 84h]
			mov ecx, dword ptr [eax]
			call ShouldHideCatalogBuilding
			test al, al
			jz keepBuilding
			popad
			popfd
			push 007F065Bh
			ret

		keepBuilding:
			popad
			popfd
			push 8
			mov dword ptr [esp + 54h], ebx
			push 007F019Ah
			ret
		}
	}

	bool HasCatalogBuildingAcceptedBytes()
	{
		constexpr std::array<uint8_t, 6> expected{0x6A, 0x08, 0x89, 0x5C, 0x24, 0x54};
		return std::memcmp(
			reinterpret_cast<const void*>(kCatalogBuildingAccepted), expected.data(), expected.size()) == 0;
	}
}

bool AdaptiveLots::Install(const bool hideRingLotMenuItems)
{
	catalogMenuFilteringInstalled = false;
	const bool valid = IsCallTo(kConstructorCall, kPlaceLotConstructor)
		&& IsCallTo(kAutomaticFacingCall, kAutomaticFacing)
		&& HasPlaceLotPrologue()
		&& IsCallTo(kTollPreviewCall, kTollPicker)
		&& IsCallTo(kTollCommitCall, kTollPicker)
		&& *reinterpret_cast<uintptr_t*>(kPlaceLotVtable + kOnKeyDownVtableSlot * sizeof(void*)) == kPlaceLotOnKeyDown
		&& *reinterpret_cast<uintptr_t*>(kPlaceLotVtable + kOnMouseMoveVtableSlot * sizeof(void*)) == kPlaceLotMouseMove;
	if (!valid)
	{
		LOG_ERROR("Adaptive lot hooks were not installed because the 1.1.641 patch bytes did not match.");
		return false;
	}

	Patcher::InstallCallHook(kConstructorCall, reinterpret_cast<void*>(&HookedConstructor));
	Patcher::InstallCallHook(kAutomaticFacingCall, reinterpret_cast<void*>(&HookedAutomaticFacing));
	Patcher::InstallHook(kPlaceLot, &HookedPlaceLot);
	originalKeyDown = reinterpret_cast<KeyDown>(
		*reinterpret_cast<uintptr_t*>(kPlaceLotVtable + kOnKeyDownVtableSlot * sizeof(void*)));
	originalMouseMove = reinterpret_cast<MouseMove>(
		*reinterpret_cast<uintptr_t*>(kPlaceLotVtable + kOnMouseMoveVtableSlot * sizeof(void*)));
	Patcher::InstallJumpTableHook(
		kPlaceLotVtable + kOnKeyDownVtableSlot * sizeof(void*), reinterpret_cast<void*>(&HookedKeyDown));
	Patcher::InstallJumpTableHook(
		kPlaceLotVtable + kOnMouseMoveVtableSlot * sizeof(void*), reinterpret_cast<void*>(&HookedMouseMove));
	Patcher::InstallCallHook(kTollPreviewCall, reinterpret_cast<void*>(&HookedTollPicker));
	Patcher::InstallCallHook(kTollCommitCall, reinterpret_cast<void*>(&HookedTollPicker));
	LOG_INFO("Installed adaptive lot hooks.");

	if (hideRingLotMenuItems)
	{
		if (HasCatalogBuildingAcceptedBytes())
		{
			Patcher::InstallHook(kCatalogBuildingAccepted, &HookedCatalogBuildingAccepted);
			catalogMenuFilteringInstalled = true;
			LOG_INFO("Installed adaptive ring menu filtering hook.");
		}
		else
		{
			LOG_ERROR(
				"Adaptive ring menu filtering was disabled because the catalog patch bytes did not match.");
		}
	}
	else
	{
		LOG_INFO("Adaptive ring menu filtering is disabled in the settings.");
	}
	return true;
}

void AdaptiveLots::LoadMappings()
{
	mappings.Clear();
	lotInfoByConfigId.clear();
	hiddenBuildingExemplarIds.clear();
	ringNameByQueryId.clear();
	selectedLotConfigId = 0;
	selectedVariant = 0;
	selectedNetwork = UINT32_MAX;
	selectedPiece = UINT32_MAX;

	cIGZPersistResourceManagerPtr resourceManager;
	if (!resourceManager)
	{
		LOG_WARN("Adaptive lot mappings are unavailable because the resource manager is unavailable.");
		return;
	}

	cRZAutoRefCount<cIGZPersistResourceKeyList> keys;
	resourceManager->GetAvailableResourceListForType(keys.AsPPObj(), LotMapping::kCohortType);
	if (!keys)
	{
		return;
	}

	for (uint32_t i = 0; i < keys->Size(); ++i)
	{
		const cGZPersistResourceKey& key = keys->GetKey(i);
		if (key.group != LotMapping::kCohortGroup)
		{
			continue;
		}

		cRZAutoRefCount<cISCResExemplarCohort> cohort;
		if (!resourceManager->GetResource(
			key, GZIID_cISCResExemplarCohort, cohort.AsPPVoid(), 0, nullptr) || !cohort)
		{
			continue;
		}

		const cISCPropertyHolder* const properties = cohort->AsISCPropertyHolder();
		const cISCProperty* const mapProperty = properties
			? properties->GetProperty(LotMapping::kPropertyNetworkPieceMap)
			: nullptr;
		const cIGZVariant* const values = mapProperty ? mapProperty->GetPropertyValue() : nullptr;
		if (!properties
			|| !values
			|| values->GetType() != cIGZVariant::Uint32Array
			|| !mappings.Add(key.instance, {values->RefUint32(), values->GetCount()}))
		{
			LOG_WARN("Ignored invalid adaptive lot mapping cohort {:08X}-{:08X}-{:08X}.",
				key.type, key.group, key.instance);
			continue;
		}

		cRZBaseString name;
		if (PropertyUtil::GetExemplarName(properties, name))
		{
			ringNameByQueryId.insert_or_assign(key.instance, name.Data());
			LOG_DEBUG("Loaded adaptive lot mapping '{}'.", name.Data());
		}
	}

	RemoveUnavailableLotConfigurations();
	RebuildLotInfoCache(resourceManager);
	LOG_INFO("Loaded adaptive lot mappings for {} query exemplar(s).", mappings.Size());
}
