// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "FlakesData.h"
#include "GameFramework/SaveGame.h"
#include "UObject/Package.h"

#include "FlakesSaveGame.generated.h"

struct FInstancedStruct;

/**
 *
 */
UCLASS()
class FLAKES_API UFlakesSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Flakes|FlakeSave")
	void SetObjectToSave(UObject* Obj);

	UFUNCTION(BlueprintCallable, Category = "Flakes|FlakeSave")
	void SetStructToSave(const FInstancedStruct& Data);

	UFUNCTION(BlueprintCallable, Category = "Flakes|FlakeSave")
	UObject* LoadObjectFromData(UObject* Outer) const;

	template <typename TClass>
	TClass* LoadObjectFromData(UObject* Outer = GetTransientPackage())
	{
		if (auto Obj = LoadObjectFromDataClassChecked(Outer, TClass::StaticClass()))
		{
			return Cast<TClass>(Obj);
		}

		return nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "Flakes|FlakeSave")
	UObject* LoadObjectFromDataClassChecked(UObject* Outer, const UClass* ExpectedClass) const;

private:
	UPROPERTY()
	FFlake Flake;

	virtual void Serialize(FArchive& Ar) override;
};