#include "LotMapping.h"

#include <array>

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
		|| table.FindWildcard(query, 1) != 0x11111111u;
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
	table.Clear();
	return failed || table.Size() != 0 || table.Contains(query);
}
