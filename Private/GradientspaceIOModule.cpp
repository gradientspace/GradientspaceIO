// Copyright Gradientspace Corp. All Rights Reserved.
#include "GradientspaceIOModule.h"

#ifdef WITH_ENGINE

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

#define LOCTEXT_NAMESPACE "FGradientspaceIOModule"


void FGradientspaceIOModule::StartupModule()
{
#ifndef GSIO_EMBEDDED_UE_BUILD
	FString BaseDir = IPluginManager::Get().FindPlugin("GradientspaceUEToolbox")->GetBaseDir();
#ifdef GSIO_USING_DEBUG
	FString DLLPath = FPaths::Combine(*BaseDir, "gradientspace_distrib/Win64/Debug/gradientspace_io.dll");
#else
	FString DLLPath = FPaths::Combine(*BaseDir, "gradientspace_distrib/Win64/Release/gradientspace_io.dll");
#endif
	if (FPaths::FileExists(*DLLPath))
		PrecompiledDLLHandle = FPlatformProcess::GetDllHandle(*DLLPath);
	else
		UE_LOG(LogTemp, Error, TEXT("Could not find GradientspaceIO DLL at %s"), *DLLPath);
#endif
}

void FGradientspaceIOModule::ShutdownModule()
{
	if (PrecompiledDLLHandle != nullptr)
		FPlatformProcess::FreeDllHandle(PrecompiledDLLHandle);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGradientspaceIOModule, GradientspaceIO)

#endif
