// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#ifdef WITH_ENGINE

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGradientspaceIOModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void* PrecompiledDLLHandle = nullptr;
};

#endif
