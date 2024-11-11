// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesModule.h"
#include "FlakesStructs.h"
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

	/*
	const FVector TestVector(FMath::VRand() * FMath::Rand());

	const FFlake FlakeFromVector = Flakes::MakeFlake(Backend, FConstStructView::Make(TestVector));

	TestEqual<UObject*>("TestStructType", Cast<UScriptStruct>(FlakeFromVector.Struct.TryLoad()), TVariantStructure<FVector>::Get());

	const FVector TestVector2;
	Flakes::WriteStruct(Backend, FStructView::Make(TestVector2), FlakeFromVector);
	*/

	const FColor TestColor = FColor::MakeRandomColor();

	const FFlake FlakeFromColor = Flakes::MakeFlake(Backend, FConstStructView::Make(TestColor));

	TestEqual<UObject*>("TestStructType", Cast<UScriptStruct>(FlakeFromColor.Struct.TryLoad()), TBaseStructure<FColor>::Get());

	FColor TestColor2;
	Flakes::WriteStruct(Backend, FStructView::Make(TestColor2), FlakeFromColor);

	TestEqual("TestValueBack", TestColor, TestColor2);

	// All tests passed.
	return true;
}