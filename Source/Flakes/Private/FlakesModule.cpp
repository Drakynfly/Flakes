// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved..

#include "FlakesModule.h"

#define LOCTEXT_NAMESPACE "FlakesModule"

const static FLazyName ModuleName("Flakes");

FFlakesModule& FFlakesModule::Get()
{
	return FModuleManager::Get().GetModuleChecked<FFlakesModule>(ModuleName);
}

void FFlakesModule::StartupModule()
{
	AddSerializationProvider(MakeUnique<Flakes::Binary::Type>());
}

void FFlakesModule::ShutdownModule()
{
}

TArray<FName> FFlakesModule::GetAllProviderNames() const
{
	TArray<FName> Out;
	SerializationProviders.GetKeys(Out);
	return Out;
}

void FFlakesModule::AddSerializationProvider(TUniquePtr<Flakes::ISerializationProvider>&& Provider)
{
	SerializationProviders.Add(Provider->GetProviderName(), MoveTemp(Provider));
}

void FFlakesModule::RemoveSerializationProvider(const FName ProviderName)
{
	SerializationProviders.Remove(ProviderName);
}

void FFlakesModule::UseSerializationProvider(const FName ProviderName, const FSerializationProviderExec& Exec) const
{
	if (const TUniquePtr<Flakes::ISerializationProvider>* Found = SerializationProviders.Find(ProviderName))
	{
		Exec(Found->Get());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFlakesModule, Flakes)