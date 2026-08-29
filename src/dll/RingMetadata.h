#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace RingMetadata
{
	struct LotInfo
	{
		uint32_t buildingExemplarId{};
		std::string displayName;
		std::string lotExemplarName;
		std::string buildingDisplayName;
	};

	using LotInfoMap = std::unordered_map<uint32_t, LotInfo>;
	using BuildingSet = std::unordered_set<uint32_t>;

	[[nodiscard]] BuildingSet BuildHiddenBuildingSet(
		std::span<const uint32_t> rootQueryIds,
		std::span<const uint32_t> replacementLotConfigIds,
		const LotInfoMap& lotInfoByConfigId);
}
