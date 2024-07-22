// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealSandboxTerrainEditor.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FUnrealSandboxTerrainEditorModule"



void FUnrealSandboxTerrainEditorModule::StartupModule() {
	UE_LOG(LogTemp, Warning, TEXT("Start FUnrealSandboxTerrainEditorModule"));

    UToolMenu* SelectionMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Select");
    FToolMenuSection& SelectionSection = SelectionMenu->AddSection(
        "MyCustomSection",
        INVTEXT("My Custom Section"),
        FToolMenuInsert("SelectBSP", EToolMenuInsertType::After)
    );
 
    SelectionSection.AddEntry(FToolMenuEntry::InitMenuEntry(
        "MyCustomEntryName",
        INVTEXT("My custom entry"),
        INVTEXT("Tooltip for my custom entry"),
        FSlateIcon(),
        FExecuteAction::CreateLambda([]() { UE_LOG(LogTemp, Log, TEXT("MyCustomEntry triggered!!")); })
    ));

}

void FUnrealSandboxTerrainEditorModule::ShutdownModule() {

}




#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealSandboxTerrainEditorModule, UnrealSandboxTerrainEditor)