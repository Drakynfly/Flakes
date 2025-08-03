// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesTestClasses.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlakesTestClasses)

bool FFlakesTestWrapperStruct::Equals(const FFlakesTestWrapperStruct& Other, FString& Result) const
{
	if (TestFloat != Other.TestFloat)
	{
		Result = TEXT("Wrapper Struct Mismatch");
		return false;
	}
	return true;
}

bool FFlakesTestCompoundStruct::Equals(const FFlakesTestCompoundStruct& Other, FString& Result) const
{
	if (TestFloat != Other.TestFloat)
	{
		Result = TEXT("Struct Float Mismatch");
		return false;
	}
	if (TestVector != Other.TestVector)
	{
		Result = TEXT("Struct Vector Mismatch");
		return false;
	}
	if (InstancedVector != Other.InstancedVector)
	{
		Result = TEXT("Struct Instanced Vector Mismatch");
		return false;
	}
	if (GameplayTag != Other.GameplayTag)
	{
		Result = TEXT("Struct Gameplay Tag Mismatch");
		return false;
	}
	if (Guid != Other.Guid)
	{
		Result = TEXT("Struct Guid Mismatch");
		return false;
	}

	return true;
}

bool UFlakesTestSimpleObject::Equals(const UFlakesTestSimpleObject* TestObject2, FString& Result) const
{
	if (TestFloat != TestObject2->TestFloat)
	{
		Result = TEXT("Float Mismatch");
		return false;
	}
	if (TestVector != TestObject2->TestVector)
	{
		Result = TEXT("Vector Mismatch");
		return false;
	}
	if (!TestWrapper.Equals(TestObject2->TestWrapper, Result))
	{
		return false;
	}
	if (!TestStruct.Equals(TestObject2->TestStruct, Result))
	{
		return false;
	}
	return true;
}

bool UFlakesTestComplexObject::Equals(const UFlakesTestComplexObject* TestObject2, FString& Result) const
{
	if (TestSimpleObjectArray.Num() != TestObject2->TestSimpleObjectArray.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("TestComplexObject::Equals: Num Mismatch"));
		return false;
	}

	if (!ObjOwnedByUs->Equals(TestObject2->ObjOwnedByUs, Result))
	{
		UE_LOG(LogTemp, Error, TEXT("TestComplexObject::Equals: ObjOwnedByUs Mismatch"));
		return false;
	}

	for (int32 i = 0; i < TestSimpleObjectArray.Num(); ++i)
	{
		TObjectPtr<UFlakesTestSimpleObject> Object1 = TestSimpleObjectArray[i];
		TObjectPtr<UFlakesTestSimpleObject> Object2 = TestObject2->TestSimpleObjectArray[i];

		// We don't want the *same* object to deserialize via name lookup. We want a new, but member-wise identical, object.
		if (Object1 == Object2)
		{
			UE_LOG(LogTemp, Error, TEXT("TestComplexObject::Equals: Array[%i] Objects are same, should be duplicate"), i);
			return false;
		}

		if (!Object1->Equals(Object2, Result))
		{
			UE_LOG(LogTemp, Error, TEXT("TestComplexObject::Equals: Array[%i] Mismatch"), i);
			return false;
		}
	}

	return true;
}