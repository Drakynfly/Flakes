// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "FlakesTestClasses.generated.h"

USTRUCT()
struct FFlakesTestSimpleStruct
{
	GENERATED_BODY()

	FFlakesTestSimpleStruct()
	{
		TestFloat = FMath::Rand();
		TestVector = FMath::VRand() * FMath::Rand();
	}

	UPROPERTY()
	float TestFloat = 0.f;

	UPROPERTY()
	FVector TestVector = FVector::ZeroVector;

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
	float TestFloat = FMath::Rand();

	UPROPERTY()
	FVector TestVector = FMath::VRand() * FMath::Rand();

	UPROPERTY()
	FFlakesTestSimpleStruct TestStruct;

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