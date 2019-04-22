#pragma once

#include "ModuleManager.h"

class PHATMERGE_API IPhATMergeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

