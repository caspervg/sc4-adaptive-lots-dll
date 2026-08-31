#include "RingMetadata.h"

#include <string>

int main()
{
	using TT = RingMetadata::TooltipText;

	// No tooltip name property at all: fallback text, with variant suffix when cycling.
	RingMetadata::LotInfo plain;
	TT t = RingMetadata::ComposeTooltipText(plain, 0xDEADBEEF, 0, 1);
	if (t.title != "Adaptive Lot 0xDEADBEEF" || !t.body.empty())
	{
		return 1;
	}
	t = RingMetadata::ComposeTooltipText(plain, 0xDEADBEEF, 2, 4);
	if (t.title != "Adaptive Lot 0xDEADBEEF (3/4)" || !t.body.empty())
	{
		return 1;
	}

	// Tooltip name key wins over the building/lot display name.
	RingMetadata::LotInfo named;
	named.displayName = "Building name";
	named.tooltipName = "Localized tooltip";
	t = RingMetadata::ComposeTooltipText(named, 0x1234, 0, 1);
	if (t.title != "Localized tooltip" || !t.body.empty())
	{
		return 1;
	}

	// Display name (building or lot chain) fallback for the title.
	RingMetadata::LotInfo buildingOnly;
	buildingOnly.displayName = "Building name";
	t = RingMetadata::ComposeTooltipText(buildingOnly, 0x1234, 0, 2);
	if (t.title != "Building name (1/2)" || !t.body.empty())
	{
		return 1;
	}

	// Present-but-empty tooltip name key: hide the whole tooltip.
	RingMetadata::LotInfo hidden;
	hidden.tooltipHidden = true;
	hidden.tooltipName = "should not show";
	hidden.displayName = "should not show either";
	hidden.tooltipDescription = "should not show either";
	t = RingMetadata::ComposeTooltipText(hidden, 0x1234, 0, 1);
	if (!t.title.empty() || !t.body.empty())
	{
		return 1;
	}

	// Description: custom tooltip description key wins.
	RingMetadata::LotInfo desc;
	desc.displayName = "Stop";
	desc.tooltipDescription = "Custom description";
	desc.buildingDescription = "Building description";
	desc.lotDescription = "Lot description";
	t = RingMetadata::ComposeTooltipText(desc, 0x1234, 0, 1);
	if (t.title != "Stop" || t.body != "Custom description")
	{
		return 1;
	}

	// Description falls back to building, then lot, description.
	RingMetadata::LotInfo descFallback;
	descFallback.displayName = "Stop";
	descFallback.buildingDescription = "Building description";
	t = RingMetadata::ComposeTooltipText(descFallback, 0x1234, 0, 1);
	if (t.body != "Building description")
	{
		return 1;
	}
	RingMetadata::LotInfo descLotOnly;
	descLotOnly.displayName = "Stop";
	descLotOnly.lotDescription = "Lot description";
	t = RingMetadata::ComposeTooltipText(descLotOnly, 0x1234, 0, 1);
	if (t.body != "Lot description")
	{
		return 1;
	}

	// Present-but-empty description key suppresses the line entirely.
	RingMetadata::LotInfo descHidden;
	descHidden.displayName = "Stop";
	descHidden.tooltipDescriptionHidden = true;
	descHidden.tooltipDescription = "should not show";
	descHidden.buildingDescription = "Building description";
	t = RingMetadata::ComposeTooltipText(descHidden, 0x1234, 0, 1);
	if (t.title != "Stop" || !t.body.empty())
	{
		return 1;
	}

	return 0;
}