#include "LotMapping.h"

#include <algorithm>
#include <ranges>
#include <tuple>
#include <utility>

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

	std::vector<Entry> entries;
	entries.reserve(values.size() / 4);
	for (size_t i = 0; i < values.size(); i += 4)
	{
		entries.push_back({values[i], values[i + 1], values[i + 2], values[i + 3]});
	}
	std::ranges::sort(entries, {}, [](const Entry& entry) {
		return std::tuple(entry.networkType, entry.pieceId, entry.variant);
	});
	for (size_t i = 1; i < entries.size(); ++i)
	{
		if (entries[i].networkType == entries[i - 1].networkType
			&& entries[i].pieceId == entries[i - 1].pieceId
			&& entries[i].variant == entries[i - 1].variant)
		{
			return false;
		}
	}
	uint32_t variant = 0;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (i == 0
			|| entries[i].networkType != entries[i - 1].networkType
			|| entries[i].pieceId != entries[i - 1].pieceId)
		{
			variant = 0;
		}
		entries[i].variant = variant++;
	}
	byQuery_.insert_or_assign(queryExemplarId, std::move(entries));
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

std::vector<uint32_t> LotMapping::Table::QueryExemplarIds() const
{
	std::vector<uint32_t> ids;
	ids.reserve(byQuery_.size());
	for (const auto& [queryExemplarId, entries] : byQuery_)
	{
		ids.push_back(queryExemplarId);
	}
	std::ranges::sort(ids);
	return ids;
}

std::vector<uint32_t> LotMapping::Table::ReplacementLotConfigIds() const
{
	std::vector<uint32_t> ids;
	for (const auto& entries : byQuery_ | std::views::values)
	{
		for (const Entry& entry : entries)
		{
			ids.push_back(entry.lotConfigId);
		}
	}
	std::ranges::sort(ids);
	const auto uniqueEnd = std::ranges::unique(ids).begin();
	ids.erase(uniqueEnd, ids.end());
	return ids;
}

void LotMapping::Table::Clear()
{
	byQuery_.clear();
}

size_t LotMapping::Table::Size() const
{
	return byQuery_.size();
}
