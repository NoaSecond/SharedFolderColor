// Copyright (c) 2026 Noa Second
// All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"
#include "Templates/Tuple.h"
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
    void OnFolderPathChanged(TArrayView<const TTuple<FString, FString>> ChangedPaths);
    void OnPathsRemoved(TConstArrayView<FStringView> RemovedPaths);
    void OnAssetRegistryFilesLoaded();
    void ScheduleExport();
    bool HasColorsChanged() const;
    bool HasAnyNonExistentFolder() const;
    bool HasSharedColorsFileChanged() const;
    FString BuildCurrentColorState() const;

    FTimerHandle AutoExportTimerHandle;
    FTimerHandle PendingExportTimerHandle;
    FDelegateHandle SetFolderColorDelegateHandle;
    FDelegateHandle FolderPathChangedDelegateHandle;
    FDelegateHandle PathsRemovedDelegateHandle;
    FDelegateHandle FilesLoadedDelegateHandle;
    FString LastExportedState;
    FString LastImportedSharedColorsState;
};
