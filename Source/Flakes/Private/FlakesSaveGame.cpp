// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesSaveGame.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlakesSaveGame)

void UFlakesSaveGame::SetObjectToSave(UObject* Obj)
{
	Flake = Flakes::CreateFlake(Obj);
}

void UFlakesSaveGame::SetStructToSave(const FInstancedStruct& Data)
{
	Flake = Flakes::CreateFlake(FConstStructView(Data));
}

UObject* UFlakesSaveGame::LoadObjectFromData(UObject* Outer) const
{
	return Flakes::CreateObject(Flake, Outer, UObject::StaticClass());
}

UObject* UFlakesSaveGame::LoadObjectFromDataClassChecked(UObject* Outer, const UClass* ExpectedClass) const
{
	if (auto&& Obj = LoadObjectFromData(Outer))
	{
		check(Obj->GetClass()->IsChildOf(ExpectedClass));
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