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
		std::string tooltipName;
		bool tooltipHidden = false;
	};

	using LotInfoMap = std::unordered_map<uint32_t, LotInfo>;
	using BuildingSet = std::unordered_set<uint32_t>;

	[[nodiscard]] BuildingSet BuildHiddenBuildingSet(
		std::span<const uint32_t> rootQueryIds,
		std::span<const uint32_t> replacementLotConfigIds,
		const LotInfoMap& lotInfoByConfigId);

	// Full tooltip text for one variant; empty when the tooltip must be hidden.
	[[nodiscard]] std::string ComposeTooltipText(
		const LotInfo& info,
		uint32_t lotConfigId,
		uint32_t variant,
		uint32_t variantCount);
}
