#pragma once

#include "Modules/ModuleManager.h"
#include "Containers/Map.h"

class FSharedFolderColorEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void LoadAndApplyColors();
    void ExportColors();
    void CheckAndExportColors();
    bool HasColorsChanged() const;

    FTimerHandle AutoExportTimerHandle;
    FString LastExportedState;
};

