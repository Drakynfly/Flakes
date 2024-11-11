// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesSaveGame.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlakesSaveGame)

void UFlakesSaveGame::SetObjectToSave(UObject* Obj)
{
	Flake = Flakes::MakeFlake(Obj);
}

void UFlakesSaveGame::SetStructToSave(const FInstancedStruct& Data)
{
	Flake = Flakes::MakeFlake(FConstStructView(Data), nullptr);
}

UObject* UFlakesSaveGame::LoadObjectFromData(UObject* Outer) const
{
	return Flakes::CreateObject(Flake, UObject::StaticClass(), Outer);
}

UObject* UFlakesSaveGame::LoadObjectFromDataClassChecked(UObject* Outer, const UClass* ExpectedClass) const
{
	if (auto&& Obj = Flakes::CreateObject(Flake, ExpectedClass, Outer))
	{
		return Obj;
	}

	return nullptr;
}

void UFlakesSaveGame::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	UE_LOG(LogFlakes, Log, TEXT("Serialize for RoseSaveFile called with Archive in state: %s"),
		Ar.IsLoading() ? TEXT("Loading") : Ar.IsSaving() ? TEXT("Saving") : TEXT("Unknown"));

	Ar << Flake;

	UE_LOG(LogFlakes, Log, TEXT("Serialize for RoseSaveFile class: %s"),
		*Flake.Struct.ToString());
}