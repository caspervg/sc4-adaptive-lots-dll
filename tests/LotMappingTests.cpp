#include "LotMapping.h"

#include <array>

int main()
{
	LotMapping::Table table;
	constexpr uint32_t query = 0x12345678;
	constexpr std::array values{
		0u, 0x00000001u, 0u, 0xEBF9A440u,
		0u, 0x00000001u, 1u, 0x8BF9B356u,
		2u, 0x00000002u, 0u, 0x8BF9B356u,
		6u, 0x00000003u, 0u, 0xABF9A45Cu,
		12u, 0x00000004u, 0u, 0x2BF9B222u,
	};
	constexpr std::array invalid{13u, 1u, 0u, 1u};
	constexpr std::array wildcard{
		LotMapping::kWildcardNetworkType, 0u, 0u, 0x11111111u,
		LotMapping::kWildcardNetworkType, 0u, 1u, 0x22222222u,
	};

	return !table.Add(query, values)
		|| !table.Add(query, wildcard)
		|| table.Add(query, invalid)
		|| table.Find(query, 0, 1, 0) != 0xEBF9A440u
		|| table.Find(query, 0, 1, 1) != 0x8BF9B356u
		|| table.Find(query, 2, 2) != 0x8BF9B356u
		|| table.Find(query, 6, 3) != 0xABF9A45Cu
		|| table.Find(query, 12, 4) != 0x2BF9B222u
		|| table.Find(query, 3, 1) != 0
		|| table.WildcardVariantCount(query) != 2
		|| table.FindWildcard(query, 1) != 0x22222222u;
}
