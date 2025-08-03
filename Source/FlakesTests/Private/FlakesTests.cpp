// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesModule.h"
#include "FlakesInterface.h"
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
		// Add regular test
		OutBeautifiedNames.Add(Provider.ToString() + TEXT("Compressed"));
		OutTestCommands.Add(Provider.ToString());

		// Add test without compression
		OutBeautifiedNames.Add(Provider.ToString() + TEXT("Raw"));
		OutTestCommands.Add(Provider.ToString() + TEXT(" -nocompress"));
	}
}

bool FlakesTests::RunTest(const FString& Parameters)
{
	// Extract the target backend
	FString BackendStr;
	if (!Parameters.Split(TEXT(" "), &BackendStr, nullptr))
	{
		BackendStr = Parameters;
	}
	const FName Backend = FName(BackendStr);

	// Extract the compression setting
	bool DisableCompression = false;
	FParse::Bool(*Parameters, TEXT("nocompress"), DisableCompression);

	Flakes::FReadOptions ReadOps;
	Flakes::FWriteOptions WriteOps;

	if (DisableCompression)
	{
		ReadOps.CompressionLevel = FOodleDataCompression::ECompressionLevel::None;
		WriteOps.SkipDecompressionStep = true;
	}

	{
		const FVector TestVector(FMath::VRand() * FMath::Rand());
		const FFlake FlakeFromVector = Flakes::MakeFlake(Backend, FConstStructView::Make(TestVector), nullptr, ReadOps);

		const UStruct* Struct = Cast<UStruct>(FlakeFromVector.Struct.TryLoad());
		const UStruct* Struct2 = TBaseStructure<FVector>::Get();

		TestEqual<const UStruct*>("TestStruct_Vector", Struct, Struct2);

		FVector TestVector2;
		Flakes::WriteStruct(Backend, FStructView::Make(TestVector2), FlakeFromVector, nullptr, WriteOps);

		TestEqual("TestValueBack_Vector", TestVector, TestVector2);

		AddInfo(TEXT("Vector Size: ") + LexToString(FlakeFromVector.Data.NumBytes()));
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
		const FFlake FlakeFromColor = Flakes::MakeFlake(Backend, FConstStructView::Make(TestColor), nullptr, ReadOps);

		const UStruct* Struct = Cast<UStruct>(FlakeFromColor.Struct.TryLoad());
		const UStruct* Struct2 = TBaseStructure<FColor>::Get();

		TestEqual<const UStruct*>("TestStruct_Color", Struct, Struct2);

		FColor TestColor2;
		Flakes::WriteStruct(Backend, FStructView::Make(TestColor2), FlakeFromColor, nullptr, WriteOps);

		TestEqual("TestValueBack_Color", TestColor, TestColor2);

		AddInfo(TEXT("Color Size: ") + LexToString(FlakeFromColor.Data.NumBytes()));
	}

	{
		UFlakesTestSimpleObject* TestObject = UFlakesTestSimpleObject::New();
		const FFlake FlakeFromObject = Flakes::MakeFlake(Backend, TestObject, ReadOps);

		const UClass* Class = Cast<UClass>(FlakeFromObject.Struct.TryLoad());
		const UClass* Class2 = UFlakesTestSimpleObject::StaticClass();

		TestEqual<const UClass*>("TestUClass", Class, Class2);

		UFlakesTestSimpleObject* TestObject2 = Flakes::CreateObject<UFlakesTestSimpleObject>(Backend, FlakeFromObject, GetTransientPackage(), WriteOps);

		FString Error;
		if (!TestTrue("TestValueBack_SimpleObject", TestObject->Equals(TestObject2, Error)))
		{
			AddInfo(Error);
		}

		AddInfo(TEXT("Simple Object Size: ") + LexToString(FlakeFromObject.Data.NumBytes()));
	}

	{
		UFlakesTestComplexObject* TestObject3 = NewObject<UFlakesTestComplexObject>();
		TestObject3->ObjOwnedByUs = UFlakesTestSimpleObject::New(TestObject3);
		//TestObject3->TestSimpleObjectArray.Add(TestObject3->ObjOwnedByUs); // @todo multiple references to own object
		for (int32 i = 0; i < 10; ++i)
		{
			TestObject3->TestSimpleObjectArray.Add(UFlakesTestSimpleObject::New(TestObject3));
		}
		const FFlake FlakeFromObject = Flakes::MakeFlake(Backend, TestObject3, ReadOps);

		const UClass* Class = Cast<UClass>(FlakeFromObject.Struct.TryLoad());
		const UClass* Class2 = UFlakesTestComplexObject::StaticClass();

		TestEqual<const UClass*>("TestUClass2", Class, Class2);

		UFlakesTestComplexObject* TestObject4 = Flakes::CreateObject<UFlakesTestComplexObject>(Backend, FlakeFromObject, GetTransientPackage(), WriteOps);

		FString Error;
		if (!TestTrue("TestValueBack_ComplexObject", TestObject3->Equals(TestObject4, Error)))
		{
			AddInfo(Error);
		}

		// @todo multiple references to own object
		/*
		if (!TestTrue("TestValueBack_ComplexObject_2", TestObject4->ObjOwnedByUs == TestObject4->TestSimpleObjectArray[0]))
		{
			AddInfo(TEXT("ObjOwnedByUs = ") + (TestObject4->ObjOwnedByUs ? TestObject4->ObjOwnedByUs->GetFullName() : "NULL"));
			AddInfo(TEXT("TestSimpleObjectArray[0] = ") + (TestObject4->TestSimpleObjectArray[0] ? TestObject4->TestSimpleObjectArray[0]->GetFullName() : "NULL"));
		}
		*/

		AddInfo(TEXT("Complex Object Size: ") + LexToString(FlakeFromObject.Data.NumBytes()));
	}

	// All tests passed.
	return true;
}