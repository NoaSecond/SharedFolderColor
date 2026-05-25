// Copyright (c) 2026 Noa Second
// All rights reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "TimerManager.h"

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
    bool HasSharedColorsFileChanged() const;
    FString BuildCurrentColorState() const;

    FTimerHandle AutoExportTimerHandle;
    FString LastExportedState;
    FString LastImportedSharedColorsState;
};
