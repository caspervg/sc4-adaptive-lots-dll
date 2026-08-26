#include "SC4AdaptiveLotsDllDirector.hpp"

#include <cIGZFrameWork.h>

#include "utils/Logger.h"
#include "utils/Settings.h"
#include "SC4VersionDetection.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <wil/win32_helpers.h>

namespace {
    constexpr uint32_t kDirectorId = 0xA0907C04u;
    static_assert(kDirectorId != 0u, "Run tools/rename_project.py or assign a unique director ID");
}

SC4AdaptiveLotsDllDirector::SC4AdaptiveLotsDllDirector() = default;
SC4AdaptiveLotsDllDirector::~SC4AdaptiveLotsDllDirector() = default;
uint32_t SC4AdaptiveLotsDllDirector::GetDirectorID() const { return kDirectorId; }

bool SC4AdaptiveLotsDllDirector::OnStart(cIGZCOM* pCOM)
{
    cRZMessage2COMDirector::OnStart(pCOM);
    if (auto* framework = RZGetFrameWork()) framework->AddHook(this);
    return true;
}

bool SC4AdaptiveLotsDllDirector::PreFrameWorkInit() { return true; }
bool SC4AdaptiveLotsDllDirector::PreAppInit() { return true; }

bool SC4AdaptiveLotsDllDirector::PostAppInit()
{
    InitializeLogger_();
    LOG_INFO("PostAppInit");
    LOG_INFO("Detected game version: {}", SC4VersionDetection::GetGameVersion());
    return true;
}

bool SC4AdaptiveLotsDllDirector::PreAppShutdown() { return true; }

bool SC4AdaptiveLotsDllDirector::PostAppShutdown()
{
    if (auto* framework = RZGetFrameWork()) framework->RemoveHook(this);
    Logger::Shutdown();
    return true;
}

bool SC4AdaptiveLotsDllDirector::PostSystemServiceShutdown() { return true; }
bool SC4AdaptiveLotsDllDirector::AbortiveQuit() { return true; }
bool SC4AdaptiveLotsDllDirector::OnInstall() { return true; }
bool SC4AdaptiveLotsDllDirector::DoMessage(cIGZMessage2*) { return true; }

std::filesystem::path SC4AdaptiveLotsDllDirector::GetUserPluginsPath_()
{
    try {
        const auto modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());
        return std::filesystem::path(modulePath.get()).parent_path();
    } catch (const wil::ResultException&) {
        return {};
    }
}

void SC4AdaptiveLotsDllDirector::InitializeLogger_()
{
    const auto pluginsPath = GetUserPluginsPath_();
    const auto settingsPath = pluginsPath / "SC4AdaptiveLotsDll.ini";
    Logger::Initialize("SC4AdaptiveLotsDll", pluginsPath.parent_path().string(), false);
    Settings settings;
    settings.Load(settingsPath);
    Logger::Shutdown();
    Logger::Initialize("SC4AdaptiveLotsDll", pluginsPath.parent_path().string(), settings.GetLogToFile());
    Logger::SetLevel(settings.GetLogLevel());
    LOG_INFO("Using settings file: {}", settingsPath.string());
}
