#include "SteamSessionHelper.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FSteamSessionHelperModule"

void FSteamSessionHelperModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("SteamSessionHelper module started."));
}

void FSteamSessionHelperModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("SteamSessionHelper module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSteamSessionHelperModule, SteamSessionHelper)
