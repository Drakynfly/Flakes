// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesTestClasses.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlakesTestClasses)

bool UFlakesTestSimpleObject::Equals(const UFlakesTestSimpleObject* TestObject2) const
{
	return TestFloat == TestObject2->TestFloat &&
			TestVector == TestObject2->TestVector &&
			TestStruct == TestObject2->TestStruct;
}

bool UFlakesTestComplexObject::Equals(const UFlakesTestComplexObject* TestObject2) const
{
	if (TestSimpleObjectArray.Num() != TestObject2->TestSimpleObjectArray.Num())
	{
		return false;
	}

	for (int32 i = 0; i < TestSimpleObjectArray.Num(); ++i)
	{
		TObjectPtr<UFlakesTestSimpleObject> Object1 = TestSimpleObjectArray[i];
		TObjectPtr<UFlakesTestSimpleObject> Object2 = TestObject2->TestSimpleObjectArray[i];

		// We don't want the *same* object to deserialize via name lookup. We want a new, but member-wise identical, object.
		if (Object1 == Object2)
		{
			return false;
		}

		if (!Object1->Equals(Object2))
		{
			return false;
		}
	}

	return TestSimpleObject->Equals(TestObject2->TestSimpleObject);
}