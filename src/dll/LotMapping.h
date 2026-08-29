#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

namespace LotMapping
{
	constexpr uint32_t kExemplarType = 0x6534284A;
	constexpr uint32_t kCohortType = 0x05342861;
	constexpr uint32_t kCohortGroup = 0xA0907C10;
	constexpr uint32_t kPropertyNetworkPieceMap = 0xA0907C13;
	constexpr uint32_t kWildcardNetworkType = UINT32_MAX;

	struct Entry
	{
		uint32_t networkType;
		uint32_t pieceId;
		uint32_t variant;
		uint32_t lotConfigId;
	};

	struct RemovedEntry
	{
		uint32_t queryExemplarId;
		Entry entry;
	};

	class Table
	{
	public:
		bool Add(uint32_t queryExemplarId, std::span<const uint32_t> values);
		[[nodiscard]] bool Contains(uint32_t queryExemplarId) const;
		[[nodiscard]] const std::vector<Entry>* Get(uint32_t queryExemplarId) const;
		[[nodiscard]] uint32_t Find(uint32_t queryExemplarId, uint32_t networkType, uint32_t pieceId, uint32_t variant = 0) const;
		[[nodiscard]] uint32_t VariantCount(uint32_t queryExemplarId, uint32_t networkType, uint32_t pieceId) const;
		[[nodiscard]] uint32_t FindWildcard(uint32_t queryExemplarId, uint32_t variant = 0) const;
		[[nodiscard]] uint32_t WildcardVariantCount(uint32_t queryExemplarId) const;
		[[nodiscard]] std::vector<uint32_t> QueryExemplarIds() const;
		[[nodiscard]] std::vector<uint32_t> ReplacementLotConfigIds() const;
		[[nodiscard]] std::vector<RemovedEntry> RemoveUnavailableLotConfigs(
			const std::function<bool(uint32_t)>& isAvailable);
		void Clear();
		[[nodiscard]] size_t Size() const;
		[[nodiscard]] size_t EntryCount() const;

	private:
		std::unordered_map<uint32_t, std::vector<Entry>> byQuery_;
	};
}
