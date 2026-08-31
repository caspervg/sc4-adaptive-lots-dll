#include "RingMetadata.h"

#include <cstdio>

RingMetadata::TooltipText RingMetadata::ComposeTooltipText(
	const LotInfo& info,
	const uint32_t lotConfigId,
	const uint32_t variant,
	const uint32_t variantCount)
{
	TooltipText result;
	if (info.tooltipHidden)
	{
		return result;
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
	result.title = name;

	if (!info.tooltipDescriptionHidden)
	{
		if (!info.tooltipDescription.empty())
		{
			result.body = info.tooltipDescription;
		}
		else if (!info.buildingDescription.empty())
		{
			result.body = info.buildingDescription;
		}
		else
		{
			result.body = info.lotDescription;
		}
	}
	return result;
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
