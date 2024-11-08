﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "FlakesStructs.h"

class FFlakesModule : public IModuleInterface
{
public:
	FLAKES_API static FFlakesModule& Get();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**     FLAKE SERIALIZATION API    **/

	TArray<FName> GetAllProviderNames() const;

	FLAKES_API void AddSerializationProvider(TUniquePtr<Flakes::ISerializationProvider>&& Provider);
	FLAKES_API void RemoveSerializationProvider(FName ProviderName);

	using FSerializationProviderExec = TFunctionRef<void(Flakes::ISerializationProvider*)>;
	void UseSerializationProvider(FName ProviderName, const FSerializationProviderExec& Exec) const;

private:
	TMap<FName, TUniquePtr<Flakes::ISerializationProvider>> SerializationProviders;
};