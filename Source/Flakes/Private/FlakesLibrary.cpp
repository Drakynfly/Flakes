// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesLibrary.h"
#include "FlakesModule.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlakesLibrary)

TArray<FString> UFlakesLibrary::GetAllProviders()
{
	TArray<FString> Out;
	Algo::Transform(FFlakesModule::Get().GetAllProviderNames(), Out,
		[](const FName Name){ return Name.ToString(); });
	return Out;
}

FFlake UFlakesLibrary::MakeFlake_Struct(const FInstancedStruct& Struct, const FName Serializer)
{
	return Flakes::MakeFlake(Serializer, Struct, nullptr);
}

FFlake UFlakesLibrary::MakeFlake(const UObject* Object, const FName Serializer)
{
	return Flakes::MakeFlake(Serializer, Object);
}

FFlake_Actor UFlakesLibrary::MakeFlake_Actor(const AActor* Actor, const FName Serializer)
{
	check(Actor);

	FFlake_Actor Flake = Flakes::MakeFlake(Serializer, Actor);
	Flake.Transform = Actor->GetTransform();

	return Flake;
}

FInstancedStruct UFlakesLibrary::ConstructStructFromFlake(const FFlake& Flake,
	const UScriptStruct* ExpectedStruct, const FName Serializer)
{
	return Flakes::CreateStruct(Serializer, Flake, ExpectedStruct);
}

UObject* UFlakesLibrary::ConstructObjectFromFlake(const FFlake& Flake, UObject* Outer,
													  const UClass* ExpectedClass, const FName Serializer)
{
	return Flakes::CreateObject(Serializer, Flake, Outer, ExpectedClass);
}

AActor* UFlakesLibrary::ConstructActorFromFlake(const FFlake_Actor& Flake, UObject* WorldContextObj,
													const TSubclassOf<AActor> ExpectedClass, const FName Serializer)
{
	if (!IsValid(WorldContextObj))
	{
		return nullptr;
	}

	UStruct* Struct = nullptr;
	if (!Flakes::Private::VerifyStruct(Flake, ExpectedClass, Struct)) return nullptr;

	const TSubclassOf<AActor> ActorClass = Cast<UClass>(Struct);

	// Unlikely because we already called VerifyStruct
	if (UNLIKELY(!IsValid(ActorClass)))
	{
		return nullptr;
	}

	if (auto&& LoadedActor = WorldContextObj->GetWorld()->SpawnActorDeferred<AActor>(ActorClass, Flake.Transform))
	{
		Flakes::WriteObject(Serializer, LoadedActor, Flake);

		LoadedActor->FinishSpawning(Flake.Transform);

		return LoadedActor;
	}

	return nullptr;
}

int64 UFlakesLibrary::GetNumBytes(const FFlake& Flake)
{
	return static_cast<int64>(Flake.Data.NumBytes());
}