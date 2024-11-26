// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesModule.h"
#include "FlakesStructs.h"
#include "FlakesTestClasses.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FlakesTests,
								 "Private.FlakesTests",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FlakesTests::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FName> Providers = FFlakesModule::Get().GetAllProviderNames();
	for (auto&& Provider : Providers)
	{
		OutBeautifiedNames.Add(Provider.ToString());
		OutTestCommands.Add(Provider.ToString());
	}
}

bool FlakesTests::RunTest(const FString& Parameters)
{
	const FName Backend = FName(Parameters);

	{
		const FVector TestVector(FMath::VRand() * FMath::Rand());
		const FFlake FlakeFromVector = Flakes::MakeFlake(Backend, FConstStructView::Make(TestVector));

		const UStruct* Struct = Cast<UStruct>(FlakeFromVector.Struct.TryLoad());
		const UStruct* Struct2 = TBaseStructure<FVector>::Get();

		TestEqual<const UStruct*>("TestStruct_Vector", Struct, Struct2);

		FVector TestVector2;
		Flakes::WriteStruct(Backend, FStructView::Make(TestVector2), FlakeFromVector);

		TestEqual("TestValueBack_Vector", TestVector, TestVector2);
	}

	/*
	{
		const FVector3f TestVector3f(FMath::VRand() * FMath::Rand());
		const FFlake FlakeFromVector3f = Flakes::MakeFlake(Backend, FConstStructView::Make(TestVector3f));

		const UStruct* Struct = Cast<UStruct>(FlakeFromVector3f.Struct.TryLoad());
		const UStruct* Struct2 = TVariantStructure<FVector3f>::Get();

		TestEqual<const UStruct*>("TestStruct_Vector3f", Struct, Struct2);

		FVector3f TestVector3f2;
		Flakes::WriteStruct(Backend, FStructView::Make(TestVector3f2), FlakeFromVector3f);

		TestEqual("TestValueBack_Vector3f", TestVector3f, TestVector3f2);
	}
	*/

	{
		const FColor TestColor = FColor::MakeRandomColor();
		const FFlake FlakeFromColor = Flakes::MakeFlake(Backend, FConstStructView::Make(TestColor));

		const UStruct* Struct = Cast<UStruct>(FlakeFromColor.Struct.TryLoad());
		const UStruct* Struct2 = TBaseStructure<FColor>::Get();

		TestEqual<const UStruct*>("TestStruct_Color", Struct, Struct2);

		FColor TestColor2;
		Flakes::WriteStruct(Backend, FStructView::Make(TestColor2), FlakeFromColor);

		TestEqual("TestValueBack_Color", TestColor, TestColor2);
	}

	{
		UFlakesTestSimpleObject* TestObject = NewObject<UFlakesTestSimpleObject>();
		const FFlake FlakeFromObject = Flakes::MakeFlake(Backend, TestObject);

		const UClass* Class = Cast<UClass>(FlakeFromObject.Struct.TryLoad());
		const UClass* Class2 = UFlakesTestSimpleObject::StaticClass();

		TestEqual<const UClass*>("TestUClass", Class, Class2);

		UFlakesTestSimpleObject* TestObject2 = Flakes::CreateObject<UFlakesTestSimpleObject>(Backend, FlakeFromObject);

		TestTrue("TestValueBack_SimpleObject", TestObject->Equals(TestObject2));
	}

	{
		UFlakesTestComplexObject* TestObject3 = NewObject<UFlakesTestComplexObject>();
		TestObject3->TestSimpleObject = NewObject<UFlakesTestSimpleObject>(TestObject3);
		for (int32 i = 0; i < 10; ++i)
		{
			TestObject3->TestSimpleObjectArray.Add(NewObject<UFlakesTestSimpleObject>(TestObject3));
		}
		const FFlake FlakeFromObject = Flakes::MakeFlake(Backend, TestObject3);

		const UClass* Class = Cast<UClass>(FlakeFromObject.Struct.TryLoad());
		const UClass* Class2 = UFlakesTestComplexObject::StaticClass();

		TestEqual<const UClass*>("TestUClass2", Class, Class2);

		UFlakesTestComplexObject* TestObject4 = Flakes::CreateObject<UFlakesTestComplexObject>(Backend, FlakeFromObject);

		TestTrue("TestValueBack_ComplexObject", TestObject3->Equals(TestObject4));
	}

	// All tests passed.
	return true;
}