// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesJsonSerializer.h"
#include "JsonObjectConverter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlakesJsonSerializer)

namespace Flakes::Json
{
#if WITH_EDITOR
	static bool ExportGuidInShortFormat = true;
	static FAutoConsoleVariableRef CVarExportGuidInShortFormat(
		TEXT("flakes.ExportGuidInShortFormat"), ExportGuidInShortFormat,
		TEXT("Import and export FGuid types in the 'Short' string format, instead of as four separate integer properties"),
		ECVF_Default);
#else
	static constexpr bool ExportGuidInShortFormat = true;
#endif

	constexpr int64 CheckFlags = 0;
	constexpr int64 SkipFlags = 0;

	namespace CopiedFromJsonObjectConverter
	{
		const FString ObjectClassNameKey = "_ClassName";
	}

	bool IsGuidStruct(const FStructProperty* Property)
	{
		static const UScriptStruct* const BaseGuidStruct = TBaseStructure<FGuid>::Get();

		return IsValid(Property->Struct) &&
			   Property->Struct->IsChildOf(BaseGuidStruct) &&
			   Property->Struct->GetPropertiesSize() == BaseGuidStruct->GetPropertiesSize();
	}

	FJsonObjectConverter::CustomExportCallback MakeJsonCustomExporter(FJsonObjectConverter::CustomExportCallback* This, const UObject* Outer)
	{
		return FJsonObjectConverter::CustomExportCallback::CreateLambda(
			[This, Outer](FProperty* Property, const void* Value) -> TSharedPtr<FJsonValue>
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if
#if !WITH_EDITOR
					  constexpr
#endif
						(ExportGuidInShortFormat)
					{
						// Export guid types a little more neatly then unreal does by default.
						if (IsGuidStruct(StructProperty))
						{
							const FGuid* GuidValue = static_cast<const FGuid*>(Value);
							return MakeShared<FJsonValueString>(GuidValue->ToString(EGuidFormats::Short));
						}
					}
				}

				// Export strong Object references as a full JsonObject if they are in the passed in Outer
				else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					const UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);

					if (Outer->HasAnyFlags(RF_WasLoaded | RF_LoadCompleted))
					{
						// Outer is loaded from disk, don't export full subobjects
						return nullptr;
					}

					// If we own the NodeObject, export by recursing over its data, e.g., export the full object data.
					if (IsValid(Object) && Object->IsInOuter(Outer))
					{
						TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();

						Out->SetStringField(CopiedFromJsonObjectConverter::ObjectClassNameKey, Object->GetClass()->GetPathName());
						if (FJsonObjectConverter::UStructToJsonObject(ObjectProperty->GetObjectPropertyValue(Value)->GetClass(),
							Object, Out, CheckFlags, SkipFlags, This))
						{
							TSharedRef<FJsonValueObject> JsonObject = MakeShared<FJsonValueObject>(Out);
							JsonObject->Type = EJson::Object;
							return JsonObject;
						}
					}
				}

				return {};
			});
	}

	FJsonObjectConverter::CustomImportCallback MakeJsonCustomImporter(FJsonObjectConverter::CustomImportCallback* This, UObject* Outer)
	{
		return FJsonObjectConverter::CustomImportCallback::CreateLambda(
			[This, Outer](const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* Value) -> bool
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if (IsGuidStruct(StructProperty))
					{
						if (FString GuidStr;
							JsonValue->TryGetString(GuidStr))
						{
							FGuid GuidValue;
							if (FGuid::ParseExact(GuidStr, EGuidFormats::Short, GuidValue))
							{
								StructProperty->SetValue_InContainer(Value, &GuidValue);
								return true;
							}
						}
					}
				}

				return false;
			});
	}

	void FSerializationProvider_Json::ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer)
	{
		FString StringData;
		FJsonObjectConverter::CustomExportCallback CustomExporter;
		CustomExporter = MakeJsonCustomExporter(&CustomExporter, Outer);

		FJsonObjectConverter::UStructToJsonObjectString(Struct.GetScriptStruct(), Struct.GetMemory(),
			StringData,
			CheckFlags,
			SkipFlags,
			0,
			&CustomExporter,
			false);

		OutData.AddUninitialized(StringData.Len());
		StringToBytes(StringData, OutData.GetData(), StringData.Len());
	}

	void FSerializationProvider_Json::ReadData(const UObject* Object, TArray<uint8>& OutData)
	{
		FString StringData;
		FJsonObjectConverter::CustomExportCallback CustomExporter;
		CustomExporter = MakeJsonCustomExporter(&CustomExporter, Object);

		FJsonObjectConverter::UStructToJsonObjectString(Object->GetClass(), Object,
			StringData,
			CheckFlags,
			SkipFlags,
			0,
			&CustomExporter,
			false);

		OutData.AddUninitialized(StringData.Len());
		StringToBytes(StringData, OutData.GetData(), StringData.Len());
	}

	void FSerializationProvider_Json::WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer)
	{
		if (!ensure(Struct.IsValid()))
		{
			return;
		}

		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(BytesToString(Data.GetData(), Data.Num()));

		TSharedPtr<FJsonObject> JsonObject;
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
		{
			return;
		}

		constexpr bool bStrictMode = false;

		FJsonObjectConverter::CustomImportCallback CustomImporter;
		CustomImporter = MakeJsonCustomImporter(&CustomImporter, Outer);

		FJsonObjectConverter::JsonObjectToUStruct(
			JsonObject.ToSharedRef(),
			Struct.GetScriptStruct(),
			Struct.GetMemory(),
			CheckFlags,
			SkipFlags,
			bStrictMode,
			nullptr,
			&CustomImporter);
	}

	void FSerializationProvider_Json::WriteData(UObject* Object, const TArray<uint8>& Data)
	{
		if (!ensure(IsValid(Object)))
		{
			return;
		}

		const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(BytesToString(Data.GetData(), Data.Num()));

		TSharedPtr<FJsonObject> JsonObject;
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
		{
			return;
		}

		constexpr bool bStrictMode = false;

		FJsonObjectConverter::CustomImportCallback CustomImporter;
		CustomImporter = MakeJsonCustomImporter(&CustomImporter, Object);

		FJsonObjectConverter::JsonObjectToUStruct(
			JsonObject.ToSharedRef(),
			Object->GetClass(),
			Object,
			CheckFlags,
			SkipFlags,
			bStrictMode,
			nullptr,
			&CustomImporter);
	}
}

FJsonObjectWrapper UFlakesJsonLibrary::CreateFlake_Struct_Json(const FInstancedStruct& Struct)
{
	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = MakeShared<FJsonObject>();
	FJsonObjectConverter::CustomExportCallback CustomExporter;
	CustomExporter = Flakes::Json::MakeJsonCustomExporter(&CustomExporter, nullptr);

	FJsonObjectConverter::UStructToJsonObject(Struct.GetScriptStruct(), Struct.GetMemory(),
		Wrapper.JsonObject.ToSharedRef(),
		Flakes::Json::CheckFlags,
		Flakes::Json::SkipFlags,
		&CustomExporter);
	return Wrapper;
}

FJsonObjectWrapper UFlakesJsonLibrary::CreateFlake_Json(const UObject* Object)
{
	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = MakeShared<FJsonObject>();
	FJsonObjectConverter::CustomExportCallback CustomExporter;
	CustomExporter = Flakes::Json::MakeJsonCustomExporter(&CustomExporter, Object);

	FJsonObjectConverter::UStructToJsonObject(Object->GetClass(), Object,
		Wrapper.JsonObject.ToSharedRef(),
		Flakes::Json::CheckFlags,
		Flakes::Json::SkipFlags,
		&CustomExporter);
	return Wrapper;
}

FJsonObjectWrapper UFlakesJsonLibrary::CreateFlake_Actor_Json(const AActor* Actor)
{
	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = MakeShared<FJsonObject>();
	FJsonObjectConverter::CustomExportCallback CustomExporter;
	CustomExporter = Flakes::Json::MakeJsonCustomExporter(&CustomExporter, Actor);

	FJsonObjectConverter::UStructToJsonObject(Actor->GetClass(), Actor,
		Wrapper.JsonObject.ToSharedRef(),
		Flakes::Json::CheckFlags,
		Flakes::Json::SkipFlags,
		&CustomExporter);
	return Wrapper;
}

UObject* UFlakesJsonLibrary::ConstructObjectFromFlake_Json(const FJsonObjectWrapper& Flake, UObject* Outer,
	const UClass* ExpectedClass)
{
	UObject* Obj = NewObject<UObject>(Outer, ExpectedClass);
	FJsonObjectConverter::CustomImportCallback CustomImporter;
	CustomImporter = Flakes::Json::MakeJsonCustomImporter(&CustomImporter, Outer);

	FJsonObjectConverter::JsonObjectToUStruct(Flake.JsonObject.ToSharedRef(), ExpectedClass,
		Obj,
		Flakes::Json::CheckFlags,
		Flakes::Json::SkipFlags,
		false,
		nullptr,
		&CustomImporter);
	return Obj;
}

AActor* UFlakesJsonLibrary::ConstructActorFromFlake_Json(const FJsonObjectWrapper& Flake, UObject* WorldContextObj,
	const TSubclassOf<AActor> ExpectedClass)
{
	if (AActor* LoadedActor = WorldContextObj->GetWorld()->SpawnActorDeferred<AActor>(
			ExpectedClass, FTransform::Identity))
	{
		FJsonObjectConverter::CustomImportCallback CustomImporter;
		CustomImporter = Flakes::Json::MakeJsonCustomImporter(&CustomImporter, LoadedActor);

		FJsonObjectConverter::JsonObjectToUStruct(Flake.JsonObject.ToSharedRef(), ExpectedClass,
			LoadedActor,
			Flakes::Json::CheckFlags,
			Flakes::Json::SkipFlags,
			false,
			nullptr,
			&CustomImporter);

		LoadedActor->FinishSpawning(LoadedActor->GetTransform());

		return LoadedActor;
	}

	return nullptr;
}

FString UFlakesJsonLibrary::ToString(const FJsonObjectWrapper& Json, const bool bPretty)
{
	FString JsonString;
	if (bPretty)
	{
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		FJsonSerializer::Serialize(Json.JsonObject.ToSharedRef(), JsonWriter);
		JsonWriter->Close();
	}
	else
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
		FJsonSerializer::Serialize(Json.JsonObject.ToSharedRef(), JsonWriter);
		JsonWriter->Close();
	}
	return JsonString;
}