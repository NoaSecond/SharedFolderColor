// Copyright (c) 2026 Noa Second
// All rights reserved.

#pragma once

#include "Delegates/Delegate.h"
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
    void OnFolderColorChanged(const FString& FolderPath);
    void ScheduleExport();
    bool HasColorsChanged() const;
    bool HasSharedColorsFileChanged() const;
    FString BuildCurrentColorState() const;

    FTimerHandle AutoExportTimerHandle;
    FTimerHandle PendingExportTimerHandle;
    FDelegateHandle SetFolderColorDelegateHandle;
    FString LastExportedState;
    FString LastImportedSharedColorsState;
};

