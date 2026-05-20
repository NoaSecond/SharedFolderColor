using UnrealBuildTool;
using System.Collections.Generic;

public class SharedFolderColorEditor : ModuleRules
{
    public SharedFolderColorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core" });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "EditorStyle",
            "UnrealEd",
            "LevelEditor",
            "ContentBrowser",
            "Json",
            "JsonUtilities"
        });
    }
}
