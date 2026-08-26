#include "LotMapping.h"

#include <algorithm>

bool LotMapping::Table::Add(const uint32_t queryExemplarId, const std::span<const uint32_t> values)
{
	if (queryExemplarId == 0 || values.empty() || values.size() % 4 != 0)
	{
		return false;
	}

	for (size_t i = 0; i < values.size(); i += 4)
	{
		if ((values[i] != kWildcardNetworkType && values[i] > 12)
			|| (values[i] == kWildcardNetworkType && values[i + 1] != 0)
			|| (values[i] != kWildcardNetworkType && values[i + 1] == 0)
			|| values[i + 3] == 0)
		{
			return false;
		}
	}

	auto& entries = byQuery_[queryExemplarId];
	for (size_t i = 0; i < values.size(); i += 4)
	{
		const Entry entry{values[i], values[i + 1], values[i + 2], values[i + 3]};
		const auto existing = std::ranges::find_if(entries, [&](const Entry& value) {
			return value.networkType == entry.networkType && value.pieceId == entry.pieceId
				&& value.variant == entry.variant;
		});
		if (existing == entries.end())
		{
			entries.push_back(entry);
		}
		else
		{
			*existing = entry;
		}
	}
	return true;
}

bool LotMapping::Table::Contains(const uint32_t queryExemplarId) const
{
	return byQuery_.contains(queryExemplarId);
}

const std::vector<LotMapping::Entry>* LotMapping::Table::Get(const uint32_t queryExemplarId) const
{
	const auto it = byQuery_.find(queryExemplarId);
	return it == byQuery_.end() ? nullptr : &it->second;
}

uint32_t LotMapping::Table::Find(
	const uint32_t queryExemplarId,
	const uint32_t networkType,
	const uint32_t pieceId, const uint32_t variant) const
{
	const auto* entries = Get(queryExemplarId);
	if (!entries)
	{
		return 0;
	}

	const auto it = std::ranges::find_if(*entries, [&](const Entry& entry) {
		return entry.networkType == networkType && entry.pieceId == pieceId && entry.variant == variant;
	});
	return it == entries->end() ? 0 : it->lotConfigId;
}

uint32_t LotMapping::Table::VariantCount(
	const uint32_t queryExemplarId, const uint32_t networkType, const uint32_t pieceId) const
{
	const auto* entries = Get(queryExemplarId);
	if (!entries)
	{
		return 0;
	}
	return static_cast<uint32_t>(std::ranges::count_if(*entries, [&](const Entry& entry) {
		return entry.networkType == networkType && entry.pieceId == pieceId;
	}));
}

uint32_t LotMapping::Table::FindWildcard(const uint32_t queryExemplarId, const uint32_t variant) const
{
	const auto* entries = Get(queryExemplarId);
	if (!entries) return 0;
	const auto it = std::ranges::find_if(*entries, [&](const Entry& entry) {
		return entry.networkType == kWildcardNetworkType && entry.pieceId == 0 && entry.variant == variant;
	});
	return it == entries->end() ? 0 : it->lotConfigId;
}

uint32_t LotMapping::Table::WildcardVariantCount(const uint32_t queryExemplarId) const
{
	const auto* entries = Get(queryExemplarId);
	if (!entries) return 0;
	return static_cast<uint32_t>(std::ranges::count_if(*entries, [](const Entry& entry) {
		return entry.networkType == kWildcardNetworkType && entry.pieceId == 0;
	}));
}

void LotMapping::Table::Clear()
{
	byQuery_.clear();
}

size_t LotMapping::Table::Size() const
{
	return byQuery_.size();
}
