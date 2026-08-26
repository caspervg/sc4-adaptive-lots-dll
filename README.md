# SC4 DLL Template

`SC4 DLL Template` is a GitHub template repository for building new SimCity 4 Win32 DLL plugins with the same basic toolchain and deployment flow used in `sc4-advanced-plop`.

It includes:

- `gzcom-dll`, `sc4-dll-utilities`, `sc4-render-services`, and `vcpkg` as git submodules
- `spdlog`, `mINI` (`pulzed-mini` in vcpkg), and `WIL` via vcpkg manifest mode
- LGPL-3.0-or-later licensing with third-party notices
- the Win32 static-library vcpkg triplet `x86-windows-static-md`
- Visual Studio 2022 Win32 debug and release presets
- automatic post-build deployment to `Documents\SimCity 4\Plugins`
- a starter `cRZMessage2COMDirector` implementation
- reusable logging, version detection, and INI settings helpers
- a minimal ImGui panel wired through `sc4-render-services`
- a GitHub Actions workflow that builds Win32 debug and release DLL artifacts
- a tag-based GitHub Actions release workflow that publishes a packaged release zip

## Quick start

1. Create a new repository from this template on GitHub and let the
   `Initialize template` action finish. It derives the C++ project name from
   the repository name and commits the initialized source once.
2. Clone it with submodules:

```powershell
git clone --recurse-submodules <your-repo-url>
cd <your-repo-directory>
```

3. Repository-name parts use the casing common to the sibling SC4 projects,
   for example `sc4-season-jumper` becomes `SC4SeasonJumper`. A `ui` or `imgui`
   part selects the ImGui starter; all other names select the standalone
   starter. To initialize manually or override that choice:

```powershell
python .\tools\rename_project.py YourDllName --ui imgui
# or infer the C++ name and UI default from a repository name:
python .\tools\rename_project.py sc4-season-jumper --repository-name
```

4. Bootstrap vcpkg:

```powershell
.\vendor\vcpkg\bootstrap-vcpkg.bat
```

The template defaults to the Win32 static-library triplet used by this codebase:

```text
x86-windows-static-md
```

5. Configure and build the DLL:

```powershell
cmake --preset vs2022-win32-debug
cmake --build --preset vs2022-win32-debug-build
```

For a release build:

```powershell
cmake --preset vs2022-win32-release
cmake --build --preset vs2022-win32-release-build
```

## Deployment

By default, the DLL target copies the built DLL into:

```text
%USERPROFILE%\Documents\SimCity 4\Plugins
```

The default INI file in `dist/` is copied only if it does not already exist in the Plugins folder, so user changes survive rebuilds.

Disable automatic deployment with:

```powershell
cmake --preset vs2022-win32-debug -DSC4_ENABLE_PLUGIN_DEPLOYMENT=OFF
```

Set a custom Plugins directory with:

```powershell
cmake --preset vs2022-win32-debug -DSC4_PLUGINS_DIR="C:/path/to/SimCity 4/Plugins"
```

The `--ui` choice is materialized by the initializer. The generated project
contains either the ImGui director and panel or the standalone non-ImGui
director; there are no ImGui conditionals in generated C++.

## CI and releases

- `build.yml` runs on pushes to `main`, pull requests, and manual dispatch to validate debug and release builds.
- `initialize.yml` runs once when GitHub creates a repository from the template, then removes itself.
- `release.yml` runs on tags matching `vMAJOR.MINOR.PATCH` and publishes a GitHub Release containing a zip with the built DLL, default INI, README, third-party notices, and upstream dependency licenses.

## Template layout

- `src/dll/`: materialized DLL source, director, and utilities
- `templates/`: ImGui and non-ImGui director variants used by the initializer
- `dist/`: default runtime INI file
- `cmake/`: helper scripts used by the build
- `tools/`: template maintenance helpers such as project renaming
- `vendor/`: git submodules

## Notes

- The template is intentionally Win32-only because SimCity 4 is a 32-bit game.
- ImGui is built in-tree from `sc4-render-services` by the main CMake build.
- `--ui none` removes the starter panel and its render-services dependency during initialization.
- The built DLL includes version metadata generated from the CMake/Git version.
- `sc4-dll-utilities` sources are compiled into the DLL; its `Logger.cpp` is excluded because the template uses its own spdlog-based logger.
- `mINI` is consumed via vcpkg as the `pulzed-mini` port and included as `mini/ini.h`.

## Customization checklist

After renaming the project, review these starter values:

- replace the demo panel and director hooks with plugin-specific logic;
- choose `--ui imgui` or `--ui none` when initializing the project;
- update the panel title, INI section, and default settings;
- choose a release version and create a `vMAJOR.MINOR.PATCH` tag;
- keep the generated director and panel IDs unless you deliberately need compatibility with an existing plugin.

The rename script replaces the starter director and panel IDs with project-specific constants, so the finished DLL does not need any ID-generation code.
The unrenamed template intentionally does not compile; this prevents accidentally shipping the placeholder IDs.
