#include "RingMetadata.h"

#include <string>

int main()
{
	// No tooltip name property at all: fallback text, with variant suffix when cycling.
	RingMetadata::LotInfo plain;
	if (RingMetadata::ComposeTooltipText(plain, 0xDEADBEEF, 0, 1) != "Adaptive Lot 0xDEADBEEF")
	{
		return 1;
	}
	if (RingMetadata::ComposeTooltipText(plain, 0xDEADBEEF, 2, 4) != "Adaptive Lot 0xDEADBEEF (3/4)")
	{
		return 1;
	}

	// Tooltip name key wins over the building/lot display name.
	RingMetadata::LotInfo named;
	named.displayName = "Building name";
	named.tooltipName = "Localized tooltip";
	if (RingMetadata::ComposeTooltipText(named, 0x1234, 0, 1) != "Localized tooltip")
	{
		return 1;
	}

	// Display name (building or lot chain) fallback.
	RingMetadata::LotInfo buildingOnly;
	buildingOnly.displayName = "Building name";
	if (RingMetadata::ComposeTooltipText(buildingOnly, 0x1234, 0, 2) != "Building name (1/2)")
	{
		return 1;
	}

	// Present-but-empty tooltip key: hide the tooltip, whatever else is set.
	RingMetadata::LotInfo hidden;
	hidden.tooltipHidden = true;
	hidden.tooltipName = "should not show";
	hidden.displayName = "should not show either";
	if (!RingMetadata::ComposeTooltipText(hidden, 0x1234, 0, 1).empty())
	{
		return 1;
	}
	if (!RingMetadata::ComposeTooltipText(hidden, 0x1234, 3, 5).empty())
	{
		return 1;
	}

	return 0;
}