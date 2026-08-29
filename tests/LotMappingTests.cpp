#include "LotMapping.h"
#include "RingMetadata.h"

#include <array>
#include <algorithm>
#include <vector>

int main()
{
	LotMapping::Table table;
	constexpr uint32_t query = 0x12345678;
	constexpr std::array values{
		0u, 0x00000001u, 5u, 0xEBF9A440u,
		0u, 0x00000001u, 9u, 0x8BF9B356u,
		2u, 0x00000002u, 0u, 0x8BF9B356u,
		6u, 0x00000003u, 0u, 0xABF9A45Cu,
		12u, 0x00000004u, 0u, 0x2BF9B222u,
		LotMapping::kWildcardNetworkType, 0u, 9u, 0x11111111u,
		LotMapping::kWildcardNetworkType, 0u, 3u, 0x22222222u,
	};
	constexpr std::array invalid{13u, 1u, 0u, 1u};
	constexpr std::array invalidWildcard{LotMapping::kWildcardNetworkType, 1u, 0u, 1u};
	constexpr std::array invalidPiece{0u, 0u, 0u, 1u};
	constexpr std::array invalidLot{0u, 1u, 0u, 0u};
	constexpr std::array invalidLength{0u, 1u, 0u};
	constexpr std::array duplicate{
		0u, 1u, 0u, 0x11111111u,
		0u, 1u, 0u, 0x22222222u,
	};
	constexpr std::array replacement{0u, 1u, 42u, 0x33333333u};

	if (!table.Add(query, values)
		|| table.Add(0, values)
		|| table.Add(query, {})
		|| table.Add(query, invalid)
		|| table.Add(query, invalidWildcard)
		|| table.Add(query, invalidPiece)
		|| table.Add(query, invalidLot)
		|| table.Add(query, invalidLength)
		|| table.Add(query, duplicate))
	{
		return 1;
	}

	bool failed =
		table.Find(query, 0, 1, 0) != 0xEBF9A440u
		|| table.Find(query, 0, 1, 1) != 0x8BF9B356u
		|| table.Find(query, 2, 2) != 0x8BF9B356u
		|| table.Find(query, 6, 3) != 0xABF9A45Cu
		|| table.Find(query, 12, 4) != 0x2BF9B222u
		|| table.Find(query, 3, 1) != 0
		|| table.WildcardVariantCount(query) != 2
		|| table.FindWildcard(query, 0) != 0x22222222u
		|| table.FindWildcard(query, 1) != 0x11111111u
		|| table.QueryExemplarIds() != std::vector<uint32_t>{query}
		|| table.ReplacementLotConfigIds() != std::vector<uint32_t>{
			0x11111111u, 0x22222222u, 0x2BF9B222u, 0x8BF9B356u, 0xABF9A45Cu, 0xEBF9A440u};

	const std::array rootLots{query, 0x44444444u};
	const std::array replacementLots{
		0xEBF9A440u, 0x8BF9B356u, 0xABF9A45Cu, 0x55555555u, 0x66666666u};
	const RingMetadata::LotInfoMap lotInfo{
		{query, {0x10000000u, "Root"}},
		{0x44444444u, {0x20000000u, "Other root"}},
		{0xEBF9A440u, {0x30000000u, "Hidden variant"}},
		{0x8BF9B356u, {0x30000000u, "Same hidden building"}},
		{0xABF9A45Cu, {0x20000000u, "Also a root"}},
		{0x55555555u, {0, "Unresolved family"}},
		{0x66666666u, {query, "Uses the unresolved root building"}},
	};
	const RingMetadata::BuildingSet hiddenBuildings = RingMetadata::BuildHiddenBuildingSet(
		rootLots, replacementLots, lotInfo);
	failed = failed
		|| hiddenBuildings.size() != 1
		|| !hiddenBuildings.contains(0x30000000u);
	LotMapping::Table replaced;
	if (!replaced.Add(query, values) || !replaced.Add(query, replacement))
	{
		return 1;
	}
	failed = failed
		|| replaced.VariantCount(query, 0, 1) != 1
		|| replaced.Find(query, 0, 1, 0) != 0x33333333u
		|| replaced.Find(query, 2, 2) != 0
		|| replaced.FindWildcard(query) != 0;

	LotMapping::Table pruned;
	constexpr uint32_t emptyQuery = 0x87654321u;
	constexpr uint32_t compactionQuery = 0x24681357u;
	constexpr std::array emptyValues{
		0u, 0x10u, 0u, 0xAAAAAAAAu,
		LotMapping::kWildcardNetworkType, 0u, 0u, 0xBBBBBBBBu,
	};
	constexpr std::array compactionValues{
		3u, 0x20u, 0u, 0xCCCCCCC0u,
		3u, 0x20u, 1u, 0xCCCCCCC1u,
		3u, 0x20u, 2u, 0xCCCCCCC2u,
	};
	if (!pruned.Add(query, values)
		|| !pruned.Add(emptyQuery, emptyValues)
		|| !pruned.Add(compactionQuery, compactionValues))
	{
		return 1;
	}
	constexpr std::array available{
		0x8BF9B356u,
		0xABF9A45Cu,
		0x11111111u,
		0xCCCCCCC0u,
		0xCCCCCCC2u,
	};
	const std::vector<LotMapping::RemovedEntry> removed =
		pruned.RemoveUnavailableLotConfigs([&](const uint32_t lotConfigId) {
			return std::ranges::find(available, lotConfigId) != available.end();
		});
	failed = failed
		|| removed.size() != 6
		|| pruned.Contains(emptyQuery)
		|| pruned.Size() != 2
		|| pruned.EntryCount() != 6
		|| pruned.VariantCount(query, 0, 1) != 1
		|| pruned.Find(query, 0, 1, 0) != 0x8BF9B356u
		|| pruned.Find(query, 0, 1, 1) != 0
		|| pruned.VariantCount(query, 2, 2) != 1
		|| pruned.Find(query, 2, 2, 0) != 0x8BF9B356u
		|| pruned.VariantCount(query, 6, 3) != 1
		|| pruned.Find(query, 6, 3, 0) != 0xABF9A45Cu
		|| pruned.VariantCount(query, 12, 4) != 0
		|| pruned.WildcardVariantCount(query) != 1
		|| pruned.FindWildcard(query, 0) != 0x11111111u
		|| pruned.FindWildcard(query, 1) != 0
		|| pruned.VariantCount(compactionQuery, 3, 0x20) != 2
		|| pruned.Find(compactionQuery, 3, 0x20, 0) != 0xCCCCCCC0u
		|| pruned.Find(compactionQuery, 3, 0x20, 1) != 0xCCCCCCC2u
		|| pruned.Find(compactionQuery, 3, 0x20, 2) != 0
		|| pruned.ReplacementLotConfigIds() != std::vector<uint32_t>{
			0x11111111u, 0x8BF9B356u, 0xABF9A45Cu, 0xCCCCCCC0u, 0xCCCCCCC2u};
	table.Clear();
	return failed || table.Size() != 0 || table.EntryCount() != 0 || table.Contains(query);
}
