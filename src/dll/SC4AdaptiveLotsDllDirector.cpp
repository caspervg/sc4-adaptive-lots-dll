#include "SC4AdaptiveLotsDllDirector.hpp"

#include <cIGZFrameWork.h>

#include "utils/Logger.h"
#include "utils/Settings.h"
#include "SC4VersionDetection.h"
#include "AdaptiveLots.h"
#include <cIGZMessage2.h>
#include <cIGZMessage2Standard.h>
#include <cIGZMessageServer2.h>
#include <GZServPtrs.h>

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
    constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1u;
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
    const uint16_t gameVersion = SC4VersionDetection::GetGameVersion();
    LOG_INFO("Detected game version: {}", gameVersion);
    if (gameVersion != 641) {
        LOG_ERROR("Adaptive lot hooks require SimCity 4 version 641.");
        return true;
    }

    try {
        AdaptiveLots::Install(settings_.GetHideRingLotMenuItems());
    } catch (const wil::ResultException& e) {
        LOG_ERROR("Failed to install adaptive lot hooks: {}", e.what());
    }

    cIGZMessageServer2Ptr messageServer;
    if (!messageServer || !messageServer->AddNotification(this, kSC4MessagePostCityInit)) {
        LOG_ERROR("Failed to subscribe to the post-city-init notification.");
    }
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
bool SC4AdaptiveLotsDllDirector::DoMessage(cIGZMessage2* message)
{
    if (message && static_cast<cIGZMessage2Standard*>(message)->GetType() == kSC4MessagePostCityInit) {
        AdaptiveLots::LoadMappings();
    }
    return true;
}

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
    settings_.Load(settingsPath);
    Logger::Shutdown();
    Logger::Initialize("SC4AdaptiveLotsDll", pluginsPath.parent_path().string(), settings_.GetLogToFile());
    Logger::SetLevel(settings_.GetLogLevel());
    LOG_INFO("Using settings file: {}", settingsPath.string());
}
