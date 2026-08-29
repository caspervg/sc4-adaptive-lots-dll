#include "RingMetadata.h"

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
