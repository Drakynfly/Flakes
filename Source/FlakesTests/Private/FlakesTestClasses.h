// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayTagsSettings.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "FlakesTestClasses.generated.h"

// A simple wrapper struct around a float
USTRUCT()
struct FFlakesTestWrapperStruct
{
	GENERATED_BODY()

	UPROPERTY()
	float TestFloat = 0.f;

	static FFlakesTestWrapperStruct Rand()
	{
		FFlakesTestWrapperStruct Struct;
		Struct.TestFloat = FMath::FRand() * FMath::Rand();
		return Struct;
	}

	bool Equals(const FFlakesTestWrapperStruct& Other, FString& Result) const;
};

USTRUCT()
struct FFlakesTestCompoundStruct
{
	GENERATED_BODY()

	UPROPERTY()
	float TestFloat = 0.f;

	UPROPERTY()
	FVector TestVector = FVector::ZeroVector;

	UPROPERTY()
	TInstancedStruct<FVector> InstancedVector;

	UPROPERTY()
	FGameplayTag GameplayTag;

	UPROPERTY()
	FGuid Guid;

	static FFlakesTestCompoundStruct Rand()
	{
		FFlakesTestCompoundStruct Struct;
		Struct.TestFloat = FMath::FRand() * FMath::Rand();
        Struct.TestVector = FMath::VRand() * FMath::Rand();
		Struct.InstancedVector.InitializeAs(FMath::VRand() * FMath::Rand());
		FGameplayTagContainer Container;
		UGameplayTagsManager::Get().RequestAllGameplayTags(Container, true);
		Struct.GameplayTag = Container.GetByIndex(FMath::RandRange(0, Container.Num()-1));
		Struct.Guid = FGuid::NewGuid();
		return Struct;
	}

	bool Equals(const FFlakesTestCompoundStruct& Other, FString& Result) const;
};

/**
 *
 */
UCLASS()
class UFlakesTestSimpleObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float TestFloat = 0.f;

	UPROPERTY()
	FVector TestVector = FVector::ZeroVector;

	UPROPERTY()
	FFlakesTestWrapperStruct TestWrapper;

	UPROPERTY()
	FFlakesTestCompoundStruct TestStruct;

	static UFlakesTestSimpleObject* New(UObject* Outer = (UObject*)GetTransientPackage())
	{
		UFlakesTestSimpleObject* SimpleObject = NewObject<UFlakesTestSimpleObject>(Outer);
		SimpleObject->TestFloat = FMath::FRand() * FMath::Rand();
		SimpleObject->TestVector = FMath::VRand() * FMath::Rand();
		SimpleObject->TestWrapper = FFlakesTestWrapperStruct::Rand();
		SimpleObject->TestStruct = FFlakesTestCompoundStruct::Rand();
		return SimpleObject;
	}

	bool Equals(const UFlakesTestSimpleObject* TestObject2, FString& Result) const;
};

/**
 *
 */
UCLASS()
class UFlakesTestComplexObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UFlakesTestSimpleObject> ObjOwnedByUs;

	UPROPERTY()
	TArray<TObjectPtr<UFlakesTestSimpleObject>> TestSimpleObjectArray;

	bool Equals(const UFlakesTestComplexObject* TestObject2, FString& Result) const;
};