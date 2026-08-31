#include "RingMetadata.h"

#include <cstdio>

std::string RingMetadata::ComposeTooltipText(
	const LotInfo& info,
	const uint32_t lotConfigId,
	const uint32_t variant,
	const uint32_t variantCount)
{
	if (info.tooltipHidden)
	{
		return {};
	}

	std::string name = info.tooltipName;
	if (name.empty())
	{
		name = info.displayName;
	}
	if (name.empty())
	{
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), "Adaptive Lot 0x%08X", lotConfigId);
		name = buffer;
	}

	if (variantCount > 1)
	{
		name += " (";
		name += std::to_string(variant + 1);
		name += '/';
		name += std::to_string(variantCount);
		name += ')';
	}
	return name;
}

RingMetadata::BuildingSet RingMetadata::BuildHiddenBuildingSet(
	const std::span<const uint32_t> rootQueryIds,
	const std::span<const uint32_t> replacementLotConfigIds,
	const LotInfoMap& lotInfoByConfigId)
{
	// Mapping query IDs are catalog building IIDs. They also normally identify the
	// root lot configuration, but preserving the raw IDs keeps unresolved roots visible.
	BuildingSet rootBuildings(rootQueryIds.begin(), rootQueryIds.end());
	for (const uint32_t queryId : rootQueryIds)
	{
		if (const auto it = lotInfoByConfigId.find(queryId);
			it != lotInfoByConfigId.end() && it->second.buildingExemplarId != 0)
		{
			rootBuildings.insert(it->second.buildingExemplarId);
		}
	}

	BuildingSet hiddenBuildings;
	for (const uint32_t lotConfigId : replacementLotConfigIds)
	{
		if (const auto it = lotInfoByConfigId.find(lotConfigId);
			it != lotInfoByConfigId.end()
			&& it->second.buildingExemplarId != 0
			&& !rootBuildings.contains(it->second.buildingExemplarId))
		{
			hiddenBuildings.insert(it->second.buildingExemplarId);
		}
	}
	return hiddenBuildings;
}
