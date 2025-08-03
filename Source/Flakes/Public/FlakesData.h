// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "FlakesData.generated.h"

USTRUCT(BlueprintType)
struct FLAKES_API FFlake
{
	GENERATED_BODY()

	// This is either a UClass, or UScriptStruct.
	UPROPERTY()
	FSoftObjectPath Struct;

	UPROPERTY()
	TArray<uint8> Data;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString DebugString;
#endif

	friend FArchive& operator<<(FArchive& Ar, FFlake& Flake)
	{
		Ar << Flake.Struct;
		Ar << Flake.Data;
#if WITH_EDITOR
		Ar << Flake.DebugString;
#endif
		return Ar;
	}
};

USTRUCT()
struct FLAKES_API FFlake_Actor : public FFlake
{
	GENERATED_BODY()

	FFlake_Actor() = default;

	FFlake_Actor(const FFlake& Flake)
	  : FFlake(Flake) {}

	UPROPERTY()
	FTransform Transform;

	friend FArchive& operator<<(FArchive& Ar, FFlake_Actor& Flake)
	{
		Ar << Flake.Struct;
		Ar << Flake.Data;
		Ar << Flake.Transform;
		return Ar;
	}
};