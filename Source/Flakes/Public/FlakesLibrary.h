// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "FlakesInterface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlakesLibrary.generated.h"

/**
 * A simple set of functions to convert arbitrary objects/actors into "Flakes", and back.
 * Flakes can be stored anywhere, written into a save file, etc., and reconstructed back into a copy of the
 * original at any time. Note that a Flake always creates a copy, it doesn't write back into the original object.
 */
UCLASS()
class FLAKES_API UFlakesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(/*  GetOptions UFunction  */)
	static TArray<FString> GetAllProviders();

public:
	/** Serialize a struct into a flake. */
	UFUNCTION(BlueprintPure, Category = "Flakes|FlakeLibrary", meta = (DisplayName = "Make Flake (Struct)"))
	static FFlake MakeFlake_Struct(const FInstancedStruct& Struct,
		UPARAM(meta=(GetOptions="Flakes.FlakesLibrary.GetAllProviders")) FName Serializer = FName("Binary"));

	/** Serialize an object (and all its subobjects) into a flake. */
	UFUNCTION(BlueprintPure, Category = "Flakes|FlakeLibrary", meta = (DisplayName = "Make Flake (Object)"))
	static FFlake MakeFlake(const UObject* Object,
		UPARAM(meta=(GetOptions="Flakes.FlakesLibrary.GetAllProviders")) FName Serializer = FName("Binary"));

	/** Serialize an actor (and all its subobjects) into a flake. */
	UFUNCTION(BlueprintPure, Category = "Flakes|FlakeLibrary", meta = (DisplayName = "Make Flake (Actor)"))
	static FFlake_Actor MakeFlake_Actor(const AActor* Actor,
		UPARAM(meta=(GetOptions="Flakes.FlakesLibrary.GetAllProviders")) FName Serializer = FName("Binary"));

	/**
	 * Attempt to read a flake back into a struct.
	 */
	UFUNCTION(BlueprintCallable, Category = "Flakes|FlakeLibrary")
	static FInstancedStruct ConstructStructFromFlake(const FFlake& Flake, const UScriptStruct* ExpectedStruct,
		UPARAM(meta=(GetOptions="Flakes.FlakesLibrary.GetAllProviders")) FName Serializer = FName("Binary"));

	/**
	 * Attempt to read a flake back into an object.
	 * Does not support actors! Use ConstructActorFromFlake for that instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Flakes|FlakeLibrary", meta = (DeterminesOutputType = "ExpectedClass"))
	static UObject* ConstructObjectFromFlake(const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass,
		UPARAM(meta=(GetOptions="Flakes.FlakesLibrary.GetAllProviders")) FName Serializer = FName("Binary"));

	/** Attempt to read a flake back into an actor. */
	UFUNCTION(BlueprintCallable, Category = "Flakes|FlakeLibrary", meta = (WorldContext = "WorldContextObj", DeterminesOutputType = "ExpectedClass"))
	static AActor* ConstructActorFromFlake(const FFlake_Actor& Flake, UObject* WorldContextObj, const TSubclassOf<AActor> ExpectedClass,
		UPARAM(meta=(GetOptions="Flakes.FlakesLibrary.GetAllProviders")) FName Serializer = FName("Binary"));

	/** Get the size of the data payload in a flake */
	UFUNCTION(BlueprintPure, Category = "Flakes|FlakeLibrary")
	static int32 GetNumBytes(const FFlake& Flake);
};