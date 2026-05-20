#include "SharedFolderColorEditor.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "JsonUtilities.h"

static FString GetSharedColorsPath()
{
    return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SharedFolderColors.json"));
}

static FString GetColorIniSection()
{
    return TEXT("SharedFolderColor.PathColors");
}

void FSharedFolderColorEditorModule::StartupModule()
{
    // Load and apply colors automatically at startup
    LoadAndApplyColors();

    // Set up a timer to check for changes every 2 seconds and auto-export
    if (GEditor)
    {
        GEditor->GetTimerManager()->SetTimer(
            AutoExportTimerHandle,
            this,
            &FSharedFolderColorEditorModule::CheckAndExportColors,
            2.0f,
            true  // Loop
        );
    }

    UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor plugin loaded - colors will be synchronized automatically"));
}

void FSharedFolderColorEditorModule::ShutdownModule()
{
    if (GEditor && AutoExportTimerHandle.IsValid())
    {
        GEditor->GetTimerManager()->ClearTimer(AutoExportTimerHandle);
    }

    // Final export before shutdown
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
                // Write all colors to the project ini so they persist and can be version controlled
                for (const auto& Pair : Root->Values)
                {
                    FString Key = Pair.Key;
                    FString Value;
                    if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
                    {
                        Value = Pair.Value->AsString();
                        GConfig->SetString(*Section, *Key, *Value, GEditorPerProjectIni);
                    }
                }
                GConfig->Flush(false, GEditorPerProjectIni);
                
                UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor: Loaded %d colors from %s"), Root->Values.Num(), *Path);
            }
        }
    }
}

void FSharedFolderColorEditorModule::CheckAndExportColors()
{
    if (HasColorsChanged())
    {
        ExportColors();
    }
}

bool FSharedFolderColorEditorModule::HasColorsChanged() const
{
    const FString Section = GetColorIniSection();
    
    TArray<FString> SectionLines;
    GConfig->GetSection(*Section, SectionLines, GEditorPerProjectIni);

    FString CurrentState;
    for (const FString& Line : SectionLines)
    {
        CurrentState += Line + TEXT("\n");
    }

    return CurrentState != LastExportedState;
}

void FSharedFolderColorEditorModule::ExportColors()
{
    const FString Section = GetColorIniSection();
    
    TArray<FString> SectionLines;
    GConfig->GetSection(*Section, SectionLines, GEditorPerProjectIni);

    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());

    for (const FString& Line : SectionLines)
    {
        FString Key, Value;
        if (Line.Split(TEXT("="), &Key, &Value))
        {
            // Remove quotes if present
            Value.RemoveFromStart(TEXT("\""));
            Value.RemoveFromEnd(TEXT("\""));
            Root->SetStringField(Key, Value);
        }
    }

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    if (FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        const FString Path = GetSharedColorsPath();
        if (FFileHelper::SaveStringToFile(Output, *Path))
        {
            LastExportedState.Empty();
            for (const FString& Line : SectionLines)
            {
                LastExportedState += Line + TEXT("\n");
            }
            UE_LOG(LogTemp, Warning, TEXT("SharedFolderColor: Auto-exported %d colors to %s"), Root->Values.Num(), *Path);
        }
    }
}

IMPLEMENT_MODULE(FSharedFolderColorEditorModule, SharedFolderColorEditor)


