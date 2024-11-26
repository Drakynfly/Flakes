// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesJsonModule.h"
#include "FlakesJsonSerializer.h"
#include "FlakesModule.h"

#define LOCTEXT_NAMESPACE "FlakesJsonModule"

void FFlakesJsonModule::StartupModule()
{
	FFlakesModule::Get().AddSerializationProvider(MakeUnique<Flakes::Json::Type>());
}

void FFlakesJsonModule::ShutdownModule()
{
	FFlakesModule::Get().RemoveSerializationProvider(Flakes::Json::ProviderName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFlakesJsonModule, FlakesJson)