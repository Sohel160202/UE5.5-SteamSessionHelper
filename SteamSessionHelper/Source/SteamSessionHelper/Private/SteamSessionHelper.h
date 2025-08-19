#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSteamSessionHelperModule : public IModuleInterface
{
public:
    /** Called when the plugin is loaded into memory */
    virtual void StartupModule() override;

    /** Called before the plugin is unloaded */
    virtual void ShutdownModule() override;
};
