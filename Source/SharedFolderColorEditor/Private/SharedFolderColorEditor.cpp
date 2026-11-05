// Copyright (c) 2026 Noa Second
// All rights reserved.

#include "SharedFolderColorEditor.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserDataSubsystem.h"
#include "IContentBrowserDataModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "JsonUtilities.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"

static bool CheckoutOrMakeWritable(const FString& FilePath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*FilePath))
    {
        return true;
    }

    if (!PlatformFile.IsReadOnly(*FilePath))
    {
        return true;
    }

    if (ISourceControlModule::Get().IsEnabled())
    {
        ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
        FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::ForceUpdate);
        if (SourceControlState.IsValid())
        {
            if (SourceControlState->IsCheckedOut())
            {
                UE_LOG(LogTemp, Log, TEXT("SharedFolderColor: File is already checked out; keeping its existing changelist: %s"), *FilePath);
                return true;
            }

            if (SourceControlState->CanCheckout())
            {
                TSharedRef<FCheckOut, ESPMode::ThreadSafe> CheckoutOp = ISourceControlOperation::Create<FCheckOut>();
                if (SourceControlProvider.Execute(CheckoutOp, FilePath) == ECommandResult::Succeeded)
                {
                    UE_LOG(LogTemp, Log, TEXT("SharedFolderColor: Successfully checked out %s"), *FilePath);
                    return true;
                }
            }
        }
    }

    if (PlatformFile.SetReadOnly(*FilePath, false))
    {
        UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor: Made file writable (cleared read-only attribute): %s"), *FilePath);
        return true;
    }

    return false;
}

static void RevertIfUnchanged(const FString& FilePath)
{
    if (ISourceControlModule::Get().IsEnabled())
    {
        ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
        FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::ForceUpdate);
        if (SourceControlState.IsValid() && SourceControlState->IsCheckedOut())
        {
            const FSourceControlChangelistPtr Changelist = SourceControlState->GetCheckInIdentifier();
            if (Changelist.IsValid())
            {
                TSharedRef<FRevertUnchanged, ESPMode::ThreadSafe> RevertOp = ISourceControlOperation::Create<FRevertUnchanged>();
                TArray<FString> Files;
                Files.Add(FilePath);
                SourceControlProvider.Execute(RevertOp, Changelist, Files);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor: Cannot revert unchanged file because its changelist is unavailable: %s"), *FilePath);
            }
        }
    }
}

static void MarkForAddIfSourceControlEnabled(const FString& FilePath)
{
    if (ISourceControlModule::Get().IsEnabled())
    {
        ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
        FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::ForceUpdate);
        if (SourceControlState.IsValid() && !SourceControlState->IsSourceControlled())
        {
            TSharedRef<FMarkForAdd, ESPMode::ThreadSafe> MarkForAddOp = ISourceControlOperation::Create<FMarkForAdd>();
            SourceControlProvider.Execute(MarkForAddOp, FilePath);
            UE_LOG(LogTemp, Log, TEXT("SharedFolderColor: Marked %s for add in source control"), *FilePath);
        }
    }
}

static FString GetSharedColorsPath()
{
    return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SharedFolderColors.json"));
}

static FString GetColorIniSection()
{
    return TEXT("PathColor");
}

static bool IsAssetRegistryReady()
{
    if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        return !AssetRegistryModule.Get().IsLoadingAssets();
    }
    return false;
}

static bool DoesFolderExist(const FString& InFolderPath)
{
    FString FolderPath = InFolderPath;
    FolderPath.TrimQuotesInline();
    FolderPath.TrimStartAndEndInline();

    if (FolderPath.IsEmpty())
    {
        return false;
    }

    // 1. Direct directory on disk (if absolute or project-relative filesystem path)
    if (FPaths::DirectoryExists(FolderPath))
    {
        return true;
    }

    // 2. Long package path converted to filesystem directory (e.g. /Game/Foo -> <ContentDir>/Foo)
    FString DiskPath;
    FString FolderPathWithSlash = FolderPath;
    if (!FolderPathWithSlash.EndsWith(TEXT("/")))
    {
        FolderPathWithSlash += TEXT("/");
    }

    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPathWithSlash, DiskPath) && FPaths::DirectoryExists(DiskPath))
    {
        return true;
    }

    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, DiskPath) && FPaths::DirectoryExists(DiskPath))
    {
        return true;
    }

    // 3. Asset Registry path check
    if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        FString FolderPathNoSlash = FolderPath;
        FolderPathNoSlash.RemoveFromEnd(TEXT("/"));

        if (AssetRegistry.PathExists(FolderPathNoSlash) || AssetRegistry.PathExists(FolderPathWithSlash))
        {
            return true;
        }
    }

    // 4. Content Browser Data Subsystem check (covers virtual and C++ class folders)
    IContentBrowserDataModule* ContentBrowserDataModule = IContentBrowserDataModule::GetPtr();
    if (ContentBrowserDataModule)
    {
        UContentBrowserDataSubsystem* Subsystem = ContentBrowserDataModule->GetSubsystem();
        if (Subsystem)
        {
            TArray<FContentBrowserItem> Items = Subsystem->GetItemsAtUserProvidedPath(FolderPath, EContentBrowserItemTypeFilter::IncludeFolders);
            if (Items.Num() > 0)
            {
                return true;
            }
        }
    }

    return false;
}

static bool AreJsonObjectsEqual(const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
{
    if (!A.IsValid() || !B.IsValid())
    {
        return A == B;
    }

    if (A->Values.Num() != B->Values.Num())
    {
        return false;
    }

    for (const auto& Pair : A->Values)
    {
        const TSharedPtr<FJsonValue>* FoundValue = B->Values.Find(Pair.Key);
        if (!FoundValue || !(*FoundValue).IsValid())
        {
            return false;
        }

        if (Pair.Value->Type != (*FoundValue)->Type)
        {
            return false;
        }

        if (Pair.Value->AsString() != (*FoundValue)->AsString())
        {
            return false;
        }
    }

    return true;
}

void FSharedFolderColorEditorModule::StartupModule()
{
    // Load and apply colors automatically at startup
    LoadAndApplyColors();

    // Listen for folder color changes so the shared JSON updates immediately.
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    SetFolderColorDelegateHandle = ContentBrowserModule.GetOnSetFolderColor().AddRaw(this, &FSharedFolderColorEditorModule::OnFolderColorChanged);

    // Listen for folder moves and renames
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    FolderPathChangedDelegateHandle = AssetViewUtils::OnFolderPathChanged().AddRaw(this, &FSharedFolderColorEditorModule::OnFolderPathChanged);

    // Listen for folder deletions and asset registry scan completion
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    PathsRemovedDelegateHandle = AssetRegistry.OnPathsRemoved().AddRaw(this, &FSharedFolderColorEditorModule::OnPathsRemoved);
    FilesLoadedDelegateHandle = AssetRegistry.OnFilesLoaded().AddRaw(this, &FSharedFolderColorEditorModule::OnAssetRegistryFilesLoaded);

    // Initial export check (will NOT checkout if file exists and content is unchanged)
    ExportColors();

    // Set up a timer to check for changes every 2 seconds and auto-export
    if (GEditor)
    {
        FTimerDelegate AutoExportDelegate;
        AutoExportDelegate = FTimerDelegate::CreateRaw(this, &FSharedFolderColorEditorModule::CheckAndExportColors);

        GEditor->GetTimerManager()->SetTimer(
            AutoExportTimerHandle,
            AutoExportDelegate,
            2.0f,
            true  // Loop
        );
    }

    UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor plugin loaded - colors will be synchronized automatically"));
}

void FSharedFolderColorEditorModule::ShutdownModule()
{
    if (SetFolderColorDelegateHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))
    {
        FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        ContentBrowserModule.GetOnSetFolderColor().Remove(SetFolderColorDelegateHandle);
        SetFolderColorDelegateHandle.Reset();
    }

    if (FolderPathChangedDelegateHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
    {
        AssetViewUtils::OnFolderPathChanged().Remove(FolderPathChangedDelegateHandle);
        FolderPathChangedDelegateHandle.Reset();
    }

    if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
    {
        FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
        if (AssetRegistryModule)
        {
            IAssetRegistry* AssetRegistry = AssetRegistryModule->TryGet();
            if (AssetRegistry)
            {
                if (PathsRemovedDelegateHandle.IsValid())
                {
                    AssetRegistry->OnPathsRemoved().Remove(PathsRemovedDelegateHandle);
                    PathsRemovedDelegateHandle.Reset();
                }
                if (FilesLoadedDelegateHandle.IsValid())
                {
                    AssetRegistry->OnFilesLoaded().Remove(FilesLoadedDelegateHandle);
                    FilesLoadedDelegateHandle.Reset();
                }
            }
        }
    }

    if (GEditor && AutoExportTimerHandle.IsValid())
    {
        GEditor->GetTimerManager()->ClearTimer(AutoExportTimerHandle);
    }

    if (GEditor && PendingExportTimerHandle.IsValid())
    {
        GEditor->GetTimerManager()->ClearTimer(PendingExportTimerHandle);
    }

    // Final export before shutdown (will NOT checkout if content is unchanged)
    ExportColors();

    UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor plugin unloaded"));
}

void FSharedFolderColorEditorModule::LoadAndApplyColors()
{
    const FString Path = GetSharedColorsPath();
    const FString Section = GetColorIniSection();

    // If the shared colors file exists, load it
    if (FPaths::FileExists(Path))
    {
        FString Input;
        if (FFileHelper::LoadFileToString(Input, *Path))
        {
            TSharedPtr<FJsonObject> Root;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Input);
            
            if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
            {
                const bool bCanPrune = IsAssetRegistryReady();
                bool bIniModified = false;

                // Write all colors to the project ini so they persist and can be version controlled
                for (const auto& Pair : Root->Values)
                {
                    FString Key = Pair.Key;
                    Key.TrimQuotesInline();
                    Key.TrimStartAndEndInline();

                    if (Key.IsEmpty())
                    {
                        continue;
                    }

                    if (bCanPrune && !DoesFolderExist(Key))
                    {
                        UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Skipped color for non-existent folder during load: '%s'"), *Key);
                        continue;
                    }

                    FString Value;
                    if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
                    {
                        Value = Pair.Value->AsString();
                        GConfig->SetString(*Section, *Key, *Value, GEditorPerProjectIni);
                        bIniModified = true;
                    }
                }

                if (bIniModified)
                {
                    GConfig->Flush(false, GEditorPerProjectIni);
                }

                LastImportedSharedColorsState = Input;
                LastExportedState = BuildCurrentColorState();
                
                UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Loaded %d colors from %s"), Root->Values.Num(), *Path);
            }
        }
    }
}

void FSharedFolderColorEditorModule::CheckAndExportColors()
{
    if (HasSharedColorsFileChanged())
    {
        LoadAndApplyColors();
    }

    if (HasColorsChanged() || (IsAssetRegistryReady() && HasAnyNonExistentFolder()))
    {
        ExportColors();
    }
}

void FSharedFolderColorEditorModule::OnFolderColorChanged(const FString& FolderPath)
{
    UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Folder color changed for '%s'"), *FolderPath);
    ScheduleExport();
}

void FSharedFolderColorEditorModule::OnFolderPathChanged(TArrayView<const TTuple<FString, FString>> ChangedPaths)
{
    const FString Section = GetColorIniSection();
    bool bModified = false;

    for (const auto& Pair : ChangedPaths)
    {
        const FString& OldPath = Pair.Key;
        const FString& NewPath = Pair.Value;

        FString ColorValue;
        if (GConfig->GetString(*Section, *OldPath, ColorValue, GEditorPerProjectIni))
        {
            GConfig->RemoveKey(*Section, *OldPath, GEditorPerProjectIni);
            GConfig->SetString(*Section, *NewPath, *ColorValue, GEditorPerProjectIni);
            bModified = true;
            UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Renamed folder color: '%s' -> '%s'"), *OldPath, *NewPath);
        }
    }

    if (bModified)
    {
        GConfig->Flush(false, GEditorPerProjectIni);
    }

    ScheduleExport();
}

void FSharedFolderColorEditorModule::OnPathsRemoved(TConstArrayView<FStringView> RemovedPaths)
{
    const FString Section = GetColorIniSection();
    bool bModified = false;

    for (const FStringView& RemovedPathView : RemovedPaths)
    {
        const FString RemovedPath(RemovedPathView);
        FString ColorValue;
        if (GConfig->GetString(*Section, *RemovedPath, ColorValue, GEditorPerProjectIni))
        {
            GConfig->RemoveKey(*Section, *RemovedPath, GEditorPerProjectIni);
            bModified = true;
            UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Removed color for deleted folder '%s'"), *RemovedPath);
        }
    }

    if (bModified)
    {
        GConfig->Flush(false, GEditorPerProjectIni);
    }

    ScheduleExport();
}

void FSharedFolderColorEditorModule::OnAssetRegistryFilesLoaded()
{
    UE_LOG(LogTemp, Verbose, TEXT("SharedFolderColor: Asset registry scan completed, verifying folder existence"));
    ScheduleExport();
}

void FSharedFolderColorEditorModule::ScheduleExport()
{
    if (!GEditor)
    {
        ExportColors();
        return;
    }

    FTimerDelegate ExportDelegate = FTimerDelegate::CreateRaw(this, &FSharedFolderColorEditorModule::ExportColors);
    GEditor->GetTimerManager()->ClearTimer(PendingExportTimerHandle);
    GEditor->GetTimerManager()->SetTimer(PendingExportTimerHandle, ExportDelegate, 0.25f, false);
}

bool FSharedFolderColorEditorModule::HasColorsChanged() const
{
    return BuildCurrentColorState() != LastExportedState;
}

bool FSharedFolderColorEditorModule::HasAnyNonExistentFolder() const
{
    if (!IsAssetRegistryReady())
    {
        return false;
    }

    const FString Section = GetColorIniSection();
    TArray<FString> SectionLines;
    GConfig->GetSection(*Section, SectionLines, GEditorPerProjectIni);

    for (const FString& Line : SectionLines)
    {
        FString Key, Value;
        if (Line.Split(TEXT("="), &Key, &Value))
        {
            Key.TrimQuotesInline();
            Key.TrimStartAndEndInline();

            if (!Key.IsEmpty() && !DoesFolderExist(Key))
            {
                return true;
            }
        }
    }

    return false;
}

bool FSharedFolderColorEditorModule::HasSharedColorsFileChanged() const
{
    const FString Path = GetSharedColorsPath();
    if (!FPaths::FileExists(Path))
    {
        return false;
    }

    FString Input;
    if (FFileHelper::LoadFileToString(Input, *Path))
    {
        return Input.Replace(TEXT("\r\n"), TEXT("\n")) != LastImportedSharedColorsState.Replace(TEXT("\r\n"), TEXT("\n"));
    }

    return false;
}

FString FSharedFolderColorEditorModule::BuildCurrentColorState() const
{
    const FString Section = GetColorIniSection();

    TArray<FString> SectionLines;
    GConfig->GetSection(*Section, SectionLines, GEditorPerProjectIni);
    SectionLines.Sort();

    FString CurrentState;
    for (const FString& Line : SectionLines)
    {
        CurrentState += Line + TEXT("\n");
    }

    return CurrentState;
}

void FSharedFolderColorEditorModule::ExportColors()
{
    const FString Section = GetColorIniSection();
    const FString Path = GetSharedColorsPath();

    TArray<FString> SectionLines;
    GConfig->GetSection(*Section, SectionLines, GEditorPerProjectIni);

    const bool bCanPrune = IsAssetRegistryReady();
    TMap<FString, FString> ValidColors;
    bool bIniModified = false;

    for (const FString& Line : SectionLines)
    {
        FString Key, Value;
        if (Line.Split(TEXT("="), &Key, &Value))
        {
            Key.TrimQuotesInline();
            Key.TrimStartAndEndInline();
            Value.TrimQuotesInline();
            Value.TrimStartAndEndInline();

            if (Key.IsEmpty())
            {
                continue;
            }

            if (bCanPrune && !DoesFolderExist(Key))
            {
                UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Removed color for non-existent folder '%s'"), *Key);
                GConfig->RemoveKey(*Section, *Key, GEditorPerProjectIni);
                bIniModified = true;
                continue;
            }

            ValidColors.Add(Key, Value);
        }
    }

    if (bIniModified)
    {
        GConfig->Flush(false, GEditorPerProjectIni);
    }

    // Sort keys alphabetically so JSON output is deterministic across machines
    TArray<FString> SortedKeys;
    ValidColors.GetKeys(SortedKeys);
    SortedKeys.Sort();

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    for (const FString& Key : SortedKeys)
    {
        Root->SetStringField(Key, ValidColors[Key]);
    }

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("SharedFolderColor: Failed to serialize colors to JSON"));
        return;
    }

    // Check if the file already exists on disk and whether its content is identical
    const bool bFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
    TSharedPtr<FJsonObject> ExistingRoot;
    if (bFileExists)
    {
        FString ExistingContent;
        if (FFileHelper::LoadFileToString(ExistingContent, *Path))
        {
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingContent);
            if (FJsonSerializer::Deserialize(Reader, ExistingRoot) && ExistingRoot.IsValid())
            {
                if (AreJsonObjectsEqual(Root, ExistingRoot))
                {
                    // JSON content is identical! Do not checkout or overwrite.
                    RevertIfUnchanged(Path);
                    LastExportedState = BuildCurrentColorState();
                    LastImportedSharedColorsState = ExistingContent;
                    return;
                }
            }
        }
    }

    // Log added, updated, or removed folder colors
    for (const auto& Pair : Root->Values)
    {
        if (!ExistingRoot.IsValid() || !ExistingRoot->Values.Contains(Pair.Key))
        {
            UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Added folder color: '%s' = %s"), *Pair.Key, *Pair.Value->AsString());
        }
        else if (ExistingRoot->Values[Pair.Key]->AsString() != Pair.Value->AsString())
        {
            UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Updated folder color: '%s' = %s"), *Pair.Key, *Pair.Value->AsString());
        }
    }

    if (ExistingRoot.IsValid())
    {
        for (const auto& Pair : ExistingRoot->Values)
        {
            if (!Root->Values.Contains(Pair.Key))
            {
                UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Removed folder color: '%s'"), *Pair.Key);
            }
        }
    }

    // File doesn't exist or content changed: checkout or make writable before saving
    if (!CheckoutOrMakeWritable(Path))
    {
        UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor: Cannot make %s writable or check it out from source control"), *Path);
        return;
    }

    if (FFileHelper::SaveStringToFile(Output, *Path))
    {
        LastExportedState = BuildCurrentColorState();
        LastImportedSharedColorsState = Output;
        UE_LOG(LogTemp, Display, TEXT("SharedFolderColor: Auto-exported %d colors to %s"), Root->Values.Num(), *Path);

        if (!bFileExists)
        {
            MarkForAddIfSourceControlEnabled(Path);
        }
        else
        {
            RevertIfUnchanged(Path);
        }
    }
}

IMPLEMENT_MODULE(FSharedFolderColorEditorModule, SharedFolderColorEditor)
