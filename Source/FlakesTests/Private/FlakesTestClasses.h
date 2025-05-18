// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "FlakesTestClasses.generated.h"

USTRUCT()
struct FFlakesTestSimpleStruct
{
	GENERATED_BODY()

	UPROPERTY()
	float TestFloat = 0.f;

	UPROPERTY()
	FVector TestVector = FVector::ZeroVector;

	static FFlakesTestSimpleStruct Rand()
	{
		FFlakesTestSimpleStruct Struct;
		Struct.TestFloat = FMath::FRand() * FMath::Rand();
        Struct.TestVector = FMath::VRand() * FMath::Rand();
		return Struct;
	}

	friend bool operator==(const FFlakesTestSimpleStruct& Lhs, const FFlakesTestSimpleStruct& RHS)
	{
		return Lhs.TestFloat == RHS.TestFloat &&
			   Lhs.TestVector == RHS.TestVector;
	}

	friend bool operator!=(const FFlakesTestSimpleStruct& Lhs, const FFlakesTestSimpleStruct& RHS)
	{
		return !(Lhs == RHS);
	}
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
	FFlakesTestSimpleStruct TestStruct;

	static UFlakesTestSimpleObject* New(UObject* Outer = (UObject*)GetTransientPackage())
	{
		UFlakesTestSimpleObject* SimpleObject = NewObject<UFlakesTestSimpleObject>(Outer);
		SimpleObject->TestFloat = FMath::FRand() * FMath::Rand();
		SimpleObject->TestVector = FMath::VRand() * FMath::Rand();
		SimpleObject->TestStruct = FFlakesTestSimpleStruct::Rand();
		return SimpleObject;
	}

	bool Equals(const UFlakesTestSimpleObject* TestObject2) const;
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
	TObjectPtr<UFlakesTestSimpleObject> TestSimpleObject;

	UPROPERTY()
	TArray<TObjectPtr<UFlakesTestSimpleObject>> TestSimpleObjectArray;

	bool Equals(const UFlakesTestComplexObject* TestObject2) const;
};