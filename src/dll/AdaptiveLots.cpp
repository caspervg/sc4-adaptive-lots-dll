#include "AdaptiveLots.h"

#include "LotMapping.h"
#include "Patcher.h"
#include "PropertyUtil.h"
#include "utils/Logger.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "GZServPtrs.h"
#include "cGZPersistResourceKey.h"
#include "cIGZPersistResourceKeyList.h"
#include "cIGZPersistResourceManager.h"
#include "cIGZVariant.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4LotConfigurationManager.h"
#include "cISC4NetworkManager.h"
#include "cISC4NetworkOccupant.h"
#include "cISC4NetworkTool.h"
#include "cISCProperty.h"
#include "cISCPropertyHolder.h"
#include "cISCResExemplar.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"

namespace
{
	static_assert(sizeof(void*) == 4, "SC4AdaptiveLotsDll requires a 32-bit build");

	constexpr uintptr_t kPlaceLotConstructor = 0x004C0430;
	constexpr uintptr_t kConstructorCall = 0x007EA49B;
	constexpr uintptr_t kAutomaticFacing = 0x004BEB70;
	constexpr uintptr_t kAutomaticFacingCall = 0x004C345E;
	constexpr uintptr_t kPlaceLot = 0x004C1740;
	constexpr uintptr_t kTollPicker = 0x004BFB30;
	constexpr uintptr_t kTollPreviewCall = 0x004C3415;
	constexpr uintptr_t kTollCommitCall = 0x004C186C;
	constexpr size_t kLotConfigIdOffset = 0x30;
	constexpr size_t kTollBoothFlagOffset = 0x150;

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
	bool genericHooksEnabled;
	bool tollHooksEnabled;
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
		const auto* entries = mappings.Get(originalQueryId);
		if (!trafficMap || !manager || !entries)
		{
			return 0;
		}

		std::array<bool, 13> probed{};
		for (const auto& entry : *entries)
		{
			if (probed[entry.networkType])
			{
				continue;
			}
			probed[entry.networkType] = true;

			if (!GetTrafficNetworkInfo(trafficMap, x, z, 1u << entry.networkType))
			{
				continue;
			}

			cISC4NetworkTool* tool = manager->GetNetworkTool(static_cast<int32_t>(entry.networkType), true);
			if (!tool)
			{
				tool = manager->GetNetworkTool(static_cast<int32_t>(entry.networkType), false);
			}
			if (!tool)
			{
				continue;
			}

			tool->Init();
			NetworkCellInfo* const cell = reinterpret_cast<NetworkTool*>(tool)->GetCellInfo(
				(static_cast<uint32_t>(z) << 16) | static_cast<uint16_t>(x));
			cISC4NetworkOccupant* const occupant = cell ? cell->networkOccupant : nullptr;
			if (!occupant || !occupant->IsOfType(static_cast<cISC4NetworkOccupant::eNetworkType>(entry.networkType)))
			{
				continue;
			}
			if (lastLoggedQuery != originalQueryId
				|| lastLoggedNetwork != entry.networkType
				|| lastLoggedPiece != occupant->PieceId())
			{
				lastLoggedQuery = originalQueryId;
				lastLoggedNetwork = entry.networkType;
				lastLoggedPiece = occupant->PieceId();
				LOG_DEBUG(
					"Adaptive query {:08X}: network {}, piece {:08X}, rotation {}, flip {}.",
					originalQueryId,
					entry.networkType,
					lastLoggedPiece,
					occupant->GetRotation(),
					occupant->GetFlip());
			}

			const uint32_t lotConfigId = mappings.Find(
				originalQueryId, entry.networkType, occupant->PieceId());
			if (!lotConfigId)
			{
				continue;
			}

			cISC4LotConfigurationManager* const lotManager = city->GetLotConfigurationManager();
			if (lotManager && lotManager->GetLotConfiguration(lotConfigId))
			{
				return lotConfigId;
			}
		}

		return 0;
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
		LOG_DEBUG("PlaceLot control {:p} created for query {:08X}.", result, queryExemplarId);
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
		if (genericHooksEnabled && control == activeControl && mappings.Contains(originalQueryId))
		{
			selectedLotConfigId = FindMappedLotConfig(control, cursorPosition);
			const uint32_t lotConfigId = selectedLotConfigId ? selectedLotConfigId : originalQueryId;
			FieldAt<uint32_t>(control, kLotConfigIdOffset) = lotConfigId;

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
		if (result && tollHooksEnabled)
		{
			const uint32_t mapped = FindMappedLotConfig(control, cursorPosition);
			if (mapped)
			{
				*lotConfigId = mapped;
			}
		}
		return result;
	}

	void __fastcall PreparePlaceLot(void* control, void*)
	{
		if (genericHooksEnabled
			&& control == activeControl
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
}

bool AdaptiveLots::Install(const bool enableGenericHooks, const bool enableTollHooks)
{
	genericHooksEnabled = enableGenericHooks;
	tollHooksEnabled = enableTollHooks;
	if (!enableGenericHooks && !enableTollHooks)
	{
		return true;
	}

	const bool valid = IsCallTo(kConstructorCall, kPlaceLotConstructor)
		&& (!enableGenericHooks || (IsCallTo(kAutomaticFacingCall, kAutomaticFacing) && HasPlaceLotPrologue()))
		&& (!enableTollHooks || (IsCallTo(kTollPreviewCall, kTollPicker) && IsCallTo(kTollCommitCall, kTollPicker)));
	if (!valid)
	{
		LOG_ERROR("Adaptive lot hooks were not installed because the 1.1.641 patch bytes did not match.");
		return false;
	}

	Patcher::InstallCallHook(kConstructorCall, reinterpret_cast<void*>(&HookedConstructor));
	if (enableGenericHooks)
	{
		Patcher::InstallCallHook(kAutomaticFacingCall, reinterpret_cast<void*>(&HookedAutomaticFacing));
		Patcher::InstallHook(kPlaceLot, &HookedPlaceLot);
	}
	if (enableTollHooks)
	{
		Patcher::InstallCallHook(kTollPreviewCall, reinterpret_cast<void*>(&HookedTollPicker));
		Patcher::InstallCallHook(kTollCommitCall, reinterpret_cast<void*>(&HookedTollPicker));
	}
	LOG_INFO("Installed adaptive lot hooks (generic={}, toll={}).", enableGenericHooks, enableTollHooks);
	return true;
}

void AdaptiveLots::LoadMappings()
{
	mappings.Clear();
	selectedLotConfigId = 0;

	cIGZPersistResourceManagerPtr resourceManager;
	if (!resourceManager)
	{
		LOG_WARN("Adaptive lot mappings are unavailable because the resource manager is unavailable.");
		return;
	}

	cRZAutoRefCount<cIGZPersistResourceKeyList> keys;
	resourceManager->GetAvailableResourceListForType(keys.AsPPObj(), LotMapping::kExemplarType);
	if (!keys)
	{
		return;
	}

	for (uint32_t i = 0; i < keys->Size(); ++i)
	{
		const cGZPersistResourceKey& key = keys->GetKey(i);
		if (key.group != LotMapping::kExemplarGroup)
		{
			continue;
		}

		cRZAutoRefCount<cISCResExemplar> exemplar;
		if (!resourceManager->GetResource(
			key, GZIID_cISCResExemplar, exemplar.AsPPVoid(), 0, nullptr) || !exemplar)
		{
			continue;
		}

		const cISCPropertyHolder* const properties = exemplar->AsISCPropertyHolder();
		uint32_t schemaVersion = 0;
		uint32_t queryExemplarId = 0;
		const cISCProperty* const mapProperty = properties
			? properties->GetProperty(LotMapping::kPropertyNetworkPieceMap)
			: nullptr;
		const cIGZVariant* const values = mapProperty ? mapProperty->GetPropertyValue() : nullptr;
		if (!properties
			|| !properties->GetProperty(LotMapping::kPropertySchemaVersion, schemaVersion)
			|| schemaVersion != LotMapping::kSchemaVersion
			|| !properties->GetProperty(LotMapping::kPropertyQueryExemplar, queryExemplarId)
			|| !values
			|| values->GetType() != cIGZVariant::Uint32Array
			|| !mappings.Add(queryExemplarId, {values->RefUint32(), values->GetCount()}))
		{
			LOG_WARN("Ignored invalid adaptive lot registry exemplar {:08X}-{:08X}-{:08X}.",
				key.type, key.group, key.instance);
			continue;
		}

		cRZBaseString name;
		if (PropertyUtil::GetExemplarName(properties, name))
		{
			LOG_DEBUG("Loaded adaptive lot mapping '{}'.", name.Data());
		}
	}

	LOG_INFO("Loaded adaptive lot mappings for {} query exemplar(s).", mappings.Size());
}
