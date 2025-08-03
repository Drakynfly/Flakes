// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesJsonSerializer.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
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

	using FOuterTracking = TSet<const UObject*>;

	namespace CopiedFromJsonObjectConverter
	{
		const FString ObjectClassNameKey = "_ClassName";
	}
	const FString ObjectStructNameKey = "_StructName";

	// Is this struct property equal to or a child with the same memory layout of a struct.
	template <typename T>
	bool IsStructCompatWith(const FStructProperty* Property)
	{
		static const UScriptStruct* const BaseGuidStruct = TBaseStructure<T>::Get();

		return Property->Struct->IsChildOf(BaseGuidStruct) &&
			   Property->Struct->GetPropertiesSize() == BaseGuidStruct->GetPropertiesSize();
	}

	// Is this struct property a wrapper over a simple numeric type
	bool IsNumericWrapperStruct(const FStructProperty* Property)
	{
		// DateTime already has custom export in JsonObjectConverter, skip this case.
		if (IsStructCompatWith<FDateTime>(Property)) return false;

		const FField* FirstProperty = Property->Struct->ChildProperties;
		if (FirstProperty && !FirstProperty->Next)
		{
			if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(FirstProperty))
			{
				return !NumericProperty->IsEnum() && Property->Struct;
			}
		}

		return false;
	}

	// Copied from ConvertScalarFPropertyToJsonValueWithContainer so we can export to structs directly
	TSharedPtr<FJsonValue> Flake_ExportNumeric(const FNumericProperty* NumericProperty, const void* Value)
	{
		// see if it's an enum
		UEnum* EnumDef = NumericProperty->GetIntPropertyEnum();
		if (EnumDef != NULL)
		{
			// export enums as strings
			FString StringValue = EnumDef->GetValueOrBitfieldAsAuthoredNameString(NumericProperty->GetSignedIntPropertyValue(Value));
			return MakeShared<FJsonValueString>(StringValue);
		}

		// We want to export numbers as numbers
		if (NumericProperty->IsFloatingPoint())
		{
			return MakeShared<FJsonValueNumber>(NumericProperty->GetFloatingPointPropertyValue(Value));
		}
		else if (NumericProperty->IsInteger())
		{
			return MakeShared<FJsonValueNumber>(NumericProperty->GetSignedIntPropertyValue(Value));
		}
		return nullptr;
	}

	// Copied from ConvertScalarJsonValueToFPropertyWithContainer so we can import to structs directly
	bool Flake_ImportNumeric(const TSharedPtr<FJsonValue>& JsonValue, const FNumericProperty* NumericProperty, void* OutValue)
	{
		if (NumericProperty->IsEnum() && JsonValue->Type == EJson::String)
		{
			// see if we were passed a string for the enum
			const UEnum* Enum = NumericProperty->GetIntPropertyEnum();
			check(Enum); // should be assured by IsEnum()
			FString StrValue = JsonValue->AsString();
			int64 IntValue = Enum->GetValueOrBitfieldFromString(StrValue, EGetByNameFlags::CheckAuthoredName);
			if (IntValue == INDEX_NONE)
			{
				//UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import enum %s from numeric value %s for property %s"), *Enum->CppType, *StrValue, *Property->GetAuthoredName());
				return false;
			}
			NumericProperty->SetIntPropertyValue(OutValue, IntValue);
		}
		if (NumericProperty->IsFloatingPoint())
		{
			// AsNumber will log an error for completely inappropriate types (then give us a default)
			NumericProperty->SetFloatingPointPropertyValue(OutValue, JsonValue->AsNumber());
			return true;
		}
		else if (NumericProperty->IsInteger())
		{
			if (JsonValue->Type == EJson::String)
			{
				// parse string -> int64 ourselves so we don't lose any precision going through AsNumber (aka double)
				NumericProperty->SetIntPropertyValue(OutValue, FCString::Atoi64(*JsonValue->AsString()));
				return true;
			}
			else
			{
				// AsNumber will log an error for completely inappropriate types (then give us a default)
				NumericProperty->SetIntPropertyValue(OutValue, (int64)JsonValue->AsNumber());
				return true;
			}
		}
		return false;
	}

	template<class CharType, class PrintPolicy>
	bool Flake_UStructToJsonObjectStringInternal(const TSharedRef<FJsonObject>& JsonObject, FString& OutJsonString, int32 Indent)
	{
		TSharedRef<TJsonWriter<CharType, PrintPolicy> > JsonWriter = TJsonWriterFactory<CharType, PrintPolicy>::Create(&OutJsonString, Indent);
		bool bSuccess = FJsonSerializer::Serialize(JsonObject, JsonWriter);
		JsonWriter->Close();
		return bSuccess;
	}

	bool Flake_UStructToJsonObjectString(const UStruct* StructDefinition, const void* Struct, FString& OutJsonString,
		const int64 InCheckFlags, const int64 InSkipFlags, const int32 Indent, const FJsonObjectConverter::CustomExportCallback* ExportCb,
		const EJsonObjectConversionFlags ConversionFlags, const bool bPrettyPrint)
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		if (FJsonObjectConverter::UStructToJsonObject(StructDefinition, Struct, JsonObject, InCheckFlags, InSkipFlags, ExportCb, ConversionFlags))
		{
			bool bSuccess = false;
			if (bPrettyPrint)
			{
				bSuccess = Flake_UStructToJsonObjectStringInternal<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>(JsonObject, OutJsonString, Indent);
			}
			else
			{
				bSuccess = Flake_UStructToJsonObjectStringInternal<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>(JsonObject, OutJsonString, Indent);
			}
			if (bSuccess)
			{
				return true;
			}
			else
			{
				UE_LOG(LogJson, Warning, TEXT("UStructToJsonObjectString - Unable to write out JSON"));
			}
		}

		return false;
	}

	TSharedPtr<FJsonValue> JsonCustomExporter(FProperty* Property, const void* Value, const FJsonObjectConverter::CustomExportCallback* This, FOuterTracking* Outers)
	{
		if (CastField<FMulticastDelegateProperty>(Property) || CastField<FMulticastDelegateProperty>(Property))
		{
			return MakeShared<FJsonValueNull>();
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (!IsValid(StructProperty->Struct))
			{
				return nullptr;
			}

			/*
			// Custom Guid Export
			if (IsStructCompatWith<FGuid>(StructProperty))
			{
				if
	#if !WITH_EDITOR
				  constexpr
	#endif
					(ExportGuidInShortFormat)
				{
					// Export guid types a little more neatly then unreal does by default.
					const FGuid* GuidValue = static_cast<const FGuid*>(Value);
					return MakeShared<FJsonValueString>(GuidValue->ToString(EGuidFormats::Short));
				}
				return nullptr;
			}
			// Export Gameplay Tags directly, (skipping internal name export, leading to shorter JSON). Imported will also handle redirects.
			else */ if (IsStructCompatWith<FGameplayTag>(StructProperty))
			{
				const FGameplayTag* GameplayTag = static_cast<const FGameplayTag*>(Value);
				return MakeShared<FJsonValueString>(GameplayTag->GetTagName().ToString());
			}
			// Export Instanced Structs as an object instead of using ExportTextItem, which the default logic uses.
			else if (IsStructCompatWith<FInstancedStruct>(StructProperty))
			{
				const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(Value);
				TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();

				if (const UScriptStruct* ScriptStruct = InstancedStruct->GetScriptStruct())
				{
					JsonObject->SetStringField(ObjectStructNameKey, ScriptStruct->GetPathName());

					if (const uint8* Memory = InstancedStruct->GetMemory())
					{
						if (!FJsonObjectConverter::UStructToJsonObject(ScriptStruct, Memory, JsonObject,
							CheckFlags, SkipFlags, This /*, ConversionFlags*/))
						{
							return nullptr;
						}
					}
				}

				return MakeShared<FJsonValueObject>(JsonObject);
			}
			else if (IsNumericWrapperStruct(StructProperty))
			{
				return Flake_ExportNumeric(CastField<FNumericProperty>(StructProperty->Struct->ChildProperties), Value);
			}
			return nullptr;
		}

		// Export strong Object references as a full JsonObject if they are owned the passed in Outer
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			const UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
			if (!IsValid(Object))
			{
				// Object is invalid.
				return nullptr;
			}

			if (Outers->Contains(Object))
			{
				// Object is already being serialized, just export the path.
				FString StringValue;
				Property->ExportTextItem_Direct(StringValue, Value, nullptr, nullptr, PPF_None);
				return MakeShared<FJsonValueString>(StringValue);
			}

			if (Object->HasAnyFlags(RF_WasLoaded | RF_LoadCompleted))
			{
				// Object is loaded from disk, don't export full subobjects
				return nullptr;
			}

			// If we can find this object in our Outers, or it's a live object, export by recursing over its data, e.g., export the full object data.
			if (Outers->Contains(Object->GetOuter()) ||
				Object->GetTypedOuter<UWorld>())
			{
				TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();

				//if (!EnumHasAnyFlags(ConversionFlags, EJsonObjectConversionFlags::SuppressClassNameForPersistentObject))
				{
					Out->SetStringField(CopiedFromJsonObjectConverter::ObjectClassNameKey, Object->GetClass()->GetPathName());
				}
				Outers->Add(Object);
				const bool Success = FJsonObjectConverter::UStructToJsonObject(
					ObjectProperty->GetObjectPropertyValue(Value)->GetClass(), Object, Out, CheckFlags, SkipFlags, This);

				if (Success)
				{
					TSharedRef<FJsonValueObject> JsonObject = MakeShared<FJsonValueObject>(Out);
					JsonObject->Type = EJson::Object;
					return JsonObject;
				}
			}
		}

		return nullptr;
	}

	FJsonObjectConverter::CustomExportCallback MakeJsonCustomExporter(const FJsonObjectConverter::CustomExportCallback* This, FOuterTracking* Outers)
	{
		return FJsonObjectConverter::CustomExportCallback::CreateStatic(&JsonCustomExporter, This, Outers);
	}

	bool JsonCustomImporter(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* Value, const FJsonObjectConverter::CustomImportCallback* This, UObject* Outer)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			/*
			// Custom Guid Import
			if (IsStructCompatWith<FGuid>(StructProperty))
			{
				if (FString GuidStr;
					JsonValue->TryGetString(GuidStr))
				{
					FGuid GuidValue;
					if (FGuid::ParseExact(GuidStr, EGuidFormats::Short, GuidValue))
					{
						Property->SetValue_InContainer(Value, &GuidValue);
						return true;
					}
				}
			}
			// Custom Gameplay Tag Import (handles tag redirectors)
			else*/ if (IsStructCompatWith<FGameplayTag>(StructProperty))
			{
				if (FString TagStr;
					JsonValue->TryGetString(TagStr))
				{
					FGameplayTag* GameplayTag = static_cast<FGameplayTag*>(Value);
					GameplayTag->FromExportString(TagStr);
					return true;
				}
			}
			// Custom Instanced Struct Import for those we wrote out as Objects.
			else if (IsStructCompatWith<FInstancedStruct>(StructProperty))
			{
				if (const TSharedPtr<FJsonObject>* JsonObject = nullptr;
					JsonValue->TryGetObject(JsonObject))
				{
					FJsonObject* Obj = JsonObject->Get();

					constexpr bool bStrictMode = false;

					const UScriptStruct* PropertyStruct = StructProperty->Struct;

					// If a specific subclass was stored in the JSON, use that instead of the PropertyClass
					const FString StructString = Obj->GetStringField(ObjectStructNameKey);
					Obj->RemoveField(ObjectStructNameKey);
					if (!StructString.IsEmpty())
					{
						const UScriptStruct* FoundClass = FPackageName::IsShortPackageName(StructString) ?
							FindFirstObject<UScriptStruct>(*StructString) : LoadObject<UScriptStruct>(nullptr, *StructString);
						if (FoundClass)
						{
							PropertyStruct = FoundClass;
						}
					}

					FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(Value);
					InstancedStruct->InitializeAs(PropertyStruct);
					if (FJsonObjectConverter::JsonObjectToUStruct(JsonObject->ToSharedRef(),
							PropertyStruct,
							InstancedStruct->GetMutableMemory(),
							CheckFlags,
							SkipFlags,
							bStrictMode,
							nullptr,
							This))
					{
						return true;
					}
				}
			}
			else if (IsNumericWrapperStruct(StructProperty))
			{
				return Flake_ImportNumeric(JsonValue, CastField<FNumericProperty>(StructProperty->Struct->ChildProperties), Value);
			}
		}

		return false;
	}

	FJsonObjectConverter::CustomImportCallback MakeJsonCustomImporter(const FJsonObjectConverter::CustomImportCallback* This, UObject* Outer)
	{
		return FJsonObjectConverter::CustomImportCallback::CreateStatic(&JsonCustomImporter, This, Outer);
	}

	void Generic_ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer, const bool UsePrettyPrint)
	{
		static const EJsonObjectConversionFlags ConversionFlags = EJsonObjectConversionFlags::WriteTextAsComplexString;

		FOuterTracking KnownOuters;
		KnownOuters.Add(Outer);
		FJsonObjectConverter::CustomExportCallback CustomExporter;
		CustomExporter = MakeJsonCustomExporter(&CustomExporter, &KnownOuters);

		FString StringData;
		Flake_UStructToJsonObjectString(Struct.GetScriptStruct(), Struct.GetMemory(),
			StringData,
			CheckFlags,
			SkipFlags,
			0,
			&CustomExporter,
			ConversionFlags,
			UsePrettyPrint);

		OutData.AddUninitialized(StringData.Len());
		StringToBytes(StringData, OutData.GetData(), StringData.Len());
	}

	void Generic_ReadData(const UObject* Object, TArray<uint8>& OutData, const bool UsePrettyPrint)
	{
		static const EJsonObjectConversionFlags ConversionFlags = EJsonObjectConversionFlags::WriteTextAsComplexString;

		FOuterTracking KnownOuters;
		KnownOuters.Add(Object);
		FJsonObjectConverter::CustomExportCallback CustomExporter;
		CustomExporter = MakeJsonCustomExporter(&CustomExporter, &KnownOuters);

		FString StringData;
		Flake_UStructToJsonObjectString(Object->GetClass(), Object,
			StringData,
			CheckFlags,
			SkipFlags,
			0,
			&CustomExporter,
			ConversionFlags,
			UsePrettyPrint);

		OutData.AddUninitialized(StringData.Len());
		StringToBytes(StringData, OutData.GetData(), StringData.Len());
	}

	void Generic_WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer)
	{
		if (!ensure(Struct.IsValid()))
		{
			return;
		}

		TSharedPtr<FJsonObject> JsonObject = nullptr;
		{
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(BytesToString(Data.GetData(), Data.Num()));
			if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
			{
				return;
			}
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

	void Generic_WriteData(UObject* Object, const TArray<uint8>& Data)
	{
		if (!ensure(IsValid(Object)))
		{
			return;
		}

		TSharedPtr<FJsonObject> JsonObject = nullptr;
		{
			const FString JsonString = BytesToString(Data.GetData(), Data.Num());
			const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
			if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
			{
				return;
			}
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

	void FSerializationProvider_Json::ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer)
	{
		Generic_ReadData(Struct, OutData, Outer, false);
	}

	void FSerializationProvider_Json::ReadData(const UObject* Object, TArray<uint8>& OutData)
	{
		Generic_ReadData(Object, OutData, false);
	}

	void FSerializationProvider_Json::WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer)
	{
		Generic_WriteData(Struct, Data, Outer);
	}

	void FSerializationProvider_Json::WriteData(UObject* Object, const TArray<uint8>& Data)
	{
		Generic_WriteData(Object, Data);
	}

	void FSerializationProvider_PrettyJson::ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer)
	{
		Generic_ReadData(Struct, OutData, Outer, true);
	}

	void FSerializationProvider_PrettyJson::ReadData(const UObject* Object, TArray<uint8>& OutData)
	{
		Generic_ReadData(Object, OutData, true);
	}

	void FSerializationProvider_PrettyJson::WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer)
	{
		Generic_WriteData(Struct, Data, Outer);
	}

	void FSerializationProvider_PrettyJson::WriteData(UObject* Object, const TArray<uint8>& Data)
	{
		Generic_WriteData(Object, Data);
	}
}

FJsonObjectWrapper UFlakesJsonLibrary::CreateFlake_Struct_Json(const FInstancedStruct& Struct)
{
	Flakes::Json::FOuterTracking KnownOuters;
	FJsonObjectConverter::CustomExportCallback CustomExporter;
	CustomExporter = Flakes::Json::MakeJsonCustomExporter(&CustomExporter, &KnownOuters);

	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = MakeShared<FJsonObject>();
	FJsonObjectConverter::UStructToJsonObject(Struct.GetScriptStruct(), Struct.GetMemory(),
		Wrapper.JsonObject.ToSharedRef(),
		Flakes::Json::CheckFlags,
		Flakes::Json::SkipFlags,
		&CustomExporter);
	return Wrapper;
}

FJsonObjectWrapper UFlakesJsonLibrary::CreateFlake_Json(const UObject* Object)
{
	Flakes::Json::FOuterTracking KnownOuters;
	FJsonObjectConverter::CustomExportCallback CustomExporter;
	CustomExporter = Flakes::Json::MakeJsonCustomExporter(&CustomExporter, &KnownOuters);

	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = MakeShared<FJsonObject>();
	FJsonObjectConverter::UStructToJsonObject(Object->GetClass(), Object,
		Wrapper.JsonObject.ToSharedRef(),
		Flakes::Json::CheckFlags,
		Flakes::Json::SkipFlags,
		&CustomExporter);
	return Wrapper;
}

FJsonObjectWrapper UFlakesJsonLibrary::CreateFlake_Actor_Json(const AActor* Actor)
{
	Flakes::Json::FOuterTracking KnownOuters;
	FJsonObjectConverter::CustomExportCallback CustomExporter;
	CustomExporter = Flakes::Json::MakeJsonCustomExporter(&CustomExporter, &KnownOuters);

	FJsonObjectWrapper Wrapper;
	Wrapper.JsonObject = MakeShared<FJsonObject>();
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