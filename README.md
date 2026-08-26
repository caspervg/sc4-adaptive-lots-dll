# SC4 Adaptive Lots DLL

SC4 Adaptive Lots DLL lets one SimCity 4 catalog building select a different lot configuration from the network piece under the cursor. This is useful for transit-enabled lots that need separate Road, Avenue, One-Way Road, SAM, or other network-specific variants without adding a separate menu item for every variant.

The DLL supports ordinary catalog-driven PlaceLot controls and the vanilla toll-booth placement path. It does not add menu items, activate the toll-booth tool, or contain hard-coded lot mappings.

## Requirements

- SimCity 4 version 1.1.641
- Windows
- One or more DBPF packages containing adaptive lot mapping exemplars

Other game versions are rejected because the hooks are specific to the 1.1.641 executable.

## Installation and use

Copy these files into `Documents\SimCity 4\Plugins`:

- `SC4AdaptiveLotsDll.dll`
- `SC4AdaptiveLotsDll.ini`
- the DBPF package containing mappings for the supported lots

Start the game normally. Select a supported building from its existing menu item and move the cursor over mapped network pieces. The preview should change to the matching lot, and clicking should place that same configuration. Unmapped tiles retain the catalog building's original lot and normal game behavior.

The adaptive-lot settings are:

```ini
[SC4AdaptiveLotsDll]
LogLevel=info
LogToFile=true
```

The DLL always installs both the ordinary PlaceLot hooks and the vanilla toll-booth picker hooks.

With the default installation location, logs are written to:

```text
Documents\SimCity 4\SC4AdaptiveLotsDll.log
```

## How it works

When the player selects a catalog building, the DLL remembers the building exemplar instance ID associated with that menu item. It keeps that original ID separate from any replacement lot configuration.

While the cursor moves, the DLL:

1. converts the cursor position to a city cell;
2. checks the traffic map for mapped network types;
3. obtains the game's network tool and network occupant for that cell;
4. reads the occupant's exact network piece instance ID;
5. looks up the catalog building ID, network type, and piece ID in the loaded mappings;
6. substitutes the mapped lot configuration for preview and placement.

The generic path replaces both the active lot configuration ID and the resolved configuration object used by the game's automatic-facing helper. The toll path changes only the lot configuration chosen by the vanilla toll picker, leaving its geometry and compatibility logic intact.

The game still performs orientation, cell coverage, geometry, validation, and placement. If the city, network data, mapping, or replacement lot configuration is unavailable, the DLL falls back to the original catalog lot.

## Building

Clone with submodules and bootstrap vcpkg:

```powershell
git clone --recurse-submodules <repository-url>
cd sc4-adaptive-lots-dll
.\vendor\vcpkg\bootstrap-vcpkg.bat
```

Build a 32-bit release DLL with Visual Studio 2022:

```powershell
cmake --preset vs2022-win32-release
cmake --build --preset vs2022-win32-release-build
```

Run the mapping tests:

```powershell
ctest --test-dir cmake-build-release-visual-studio -C Release --output-on-failure
```

The build deploys the DLL to `Documents\SimCity 4\Plugins` by default. The default INI is copied only when one is not already present.

Disable deployment or choose another Plugins directory during configuration:

```powershell
cmake --preset vs2022-win32-release -DSC4_ENABLE_PLUGIN_DEPLOYMENT=OFF
cmake --preset vs2022-win32-release -DSC4_PLUGINS_DIR="C:/path/to/SimCity 4/Plugins"
```

CLion can open the repository's root `CMakeLists.txt` directly. Use a 32-bit MSVC toolchain/profile; the DLL cannot be built as x64.

## Adding support for a lot

Mappings are ordinary exemplar resources stored in a DBPF package. Each mapping exemplar uses:

| Field | Value |
|---|---|
| Resource type | `0x6534284A` |
| Resource group | `0xA0907C10` |
| Resource instance | Any unique IID |
| `0xA0907C12` | Catalog Building Exemplar ID; one `Uint32` IID |
| `0xA0907C13` | Network/variant mappings; a repeating `Uint32` array |

Property `0xA0907C13` is a sequence of quadruples:

```text
network type, exact network piece IID, variant index, replacement lot configuration IID
```

For example, these entries select three different lot configurations for one catalog building:

```text
0x00000000, 0x00004B00, 0, 0x0C9DC024   # Road / variant 0
0x00000000, 0x00004B00, 1, 0x0C9DC023   # Road / variant 1
0x00000003, 0x5E54B200, 0, 0x0C9DBFE0   # Street / SAM 2
0x00000003, 0x5E54B900, 0, 0x0C9DBFFA   # Street / SAM 9
```

For a network/piece ring, add consecutive zero-based variant indices. While the placement tool is active, `Tab` advances to the next variant and `Alt+Tab` moves backward; the DLL immediately reruns the vanilla mouse-move path so the preview effect and props are rebuilt from the selected lot configuration.

The network type is the game's numeric network ID from 0 through 12. Common values include Road `0`, Rail `1`, Highway `2`, Street `3`, Avenue `6`, Light Rail `8`, Monorail `9`, One-Way Road `10`, Dirt Road `11`, and Ground Highway `12`.

The exact piece IID is important: override networks such as SAM share the Street base network type but expose different piece IDs. Rotation and flip are reported for diagnosis but are not part of the mapping key.

Multiple mapping exemplars may use the same catalog building exemplar ID; their entries are merged. Variant indices form a zero-based ring for each network-type and piece-ID pair. Avoid defining the same network-type, piece-ID, and variant more than once.

An entire mapping exemplar is ignored when:

- the catalog building exemplar ID is zero;
- the mapping array is empty or is not a multiple of three;
- a network type is greater than 12;
- a piece IID or replacement lot configuration IID is zero;
- the schema version is missing or unsupported.

## Finding the required IDs

The easiest probe is the DLL's debug log:

1. Set `LogLevel=debug` in `SC4AdaptiveLotsDll.ini`.
2. Create a temporary mapping exemplar for the catalog building.
3. Add one placeholder quadruple for each network type you want to inspect. Use a nonzero placeholder piece IID, variant `0`, and an existing lot configuration IID.
4. Restart the game, select the menu item, and hover over the target network pieces.
5. Read `SC4AdaptiveLotsDll.log`.

Selecting the menu item produces a line similar to:

```text
PlaceLot control ... created for query 0C9DC024.
```

Here `0C9DC024` is the catalog building exemplar ID expected by property `0xA0907C12`. Hovering over a probed network produces:

```text
Adaptive query 0C9DC024: network 3, piece 5E54B200, rotation 0, flip 0.
```

Use network `3` and piece `0x5E54B200` in a real mapping quadruple, then replace the placeholder lot value with the IID of the matching lot configuration exemplar. Add additional variant indices for alternate footprints or transit-switch configurations.

Lot configuration IIDs can be found by inspecting the existing variants in Reader or another DBPF editor. Verify that each replacement is a complete, working lot configuration with compatible geometry and transit behavior; the DLL selects configurations but does not repair incompatible lots.

Restart SimCity 4 after adding or changing a mapping package so the game reloads the DBPF resources.

## Troubleshooting

- **The log says zero catalog mappings were loaded:** check the exemplar resource group and all mapping property IDs, then restart the game.
- **The network and piece appear in the debug log but the preview does not change:** verify that the quadruple contains that exact pair and that the replacement lot configuration IID exists.
- **The preview changes but placement fails:** test the replacement lot normally and check its size, transit switches, paths, and placement requirements.
- **The hooks are not installed:** confirm that the executable is SimCity 4 version 1.1.641 and check the log for a patch-byte mismatch.
- **An unmapped network behaves normally:** this is intentional fallback behavior.

## License

This project is licensed under LGPL-3.0-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
