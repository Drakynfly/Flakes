// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesInterface.h"
#include "FlakesLogging.h"
#include "FlakesModule.h"

#include "Compression/OodleDataCompressionUtil.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#endif

#if WITH_EDITOR
TAutoConsoleVariable<bool> CVarLogCompressionStatistics{
	TEXT("flakes.LogCompressionStatistics"),
	false,
	TEXT("Log before/after byte counts when compressing flakes")
};
#endif

namespace Flakes
{
	namespace Private
	{
		bool VerifyStruct(const FFlake& Flake, const UStruct* Expected, UStruct*& OutStruct)
		{
			if (!ensureMsgf(IsValid(Expected), TEXT("VerifyStruct: Invalid Expected Type. Prefer passing UObject::StaticClass(), over nullptr")))
			{
				return false;
			}

			OutStruct = Cast<UStruct>(Flake.Struct.TryLoad());
			if (!IsValid(OutStruct))
			{
				UE_LOG(LogFlakes, Error, TEXT("VerifyStruct: Invalid Struct Type. Failed to load, or was null."));
				return false;
			}

			if (!OutStruct->IsChildOf(Expected))
			{
				UE_LOG(LogFlakes, Error, TEXT("VerifyStruct: StructType does match Expected type"));
				return false;
			}

			return true;
		}

		bool CompressFlake(FFlake& Flake, TArray<uint8>&& Raw, const FReadOptions& Options)
		{
			if (Options.CompressionLevel == FOodleDataCompression::ECompressionLevel::None)
			{
				Flake.Data = MoveTemp(Raw);
				return true;
			}

#if WITH_EDITOR
			const auto BeforeCompression = Raw.NumBytes();
#endif

			const bool Success = FOodleCompressedArray::CompressTArray(Flake.Data, Raw, Options.Compressor, Options.CompressionLevel);
			if (!Success)
			{
				UE_LOG(LogFlakes, Error, TEXT("CompressFlake failed!"))
			}
#if WITH_EDITOR
			else
			{
				const auto AfterCompression = Flake.Data.NumBytes();
				if (CVarLogCompressionStatistics.GetValueOnGameThread())
				{
					UE_LOG(LogFlakes, Log, TEXT("[Flake Compression Log]: Compressed '%llu' bytes to '%llu' bytes"), BeforeCompression, AfterCompression);
				}
			}
#endif

			return Success;
		}

		bool DecompressFlake(const FFlake& Flake, TArray<uint8>& Raw, const FWriteOptions& Options)
		{
			if (Options.SkipDecompressionStep)
			{
				// @todo remove this copy please
				Raw = Flake.Data;
				return true;
			}

			const bool Success = FOodleCompressedArray::DecompressToTArray(Raw, Flake.Data);
			if (!Success)
			{
				UE_LOG(LogFlakes, Error, TEXT("DecompressFlake failed!"))
			}
			return Success;
		}

		void PostLoadStruct(const FStructView& Struct)
		{
			check(Struct.GetScriptStruct())
			UScriptStruct::ICppStructOps* StructOps = Struct.GetScriptStruct()->GetCppStructOps();
			if (StructOps->HasPostScriptConstruct())
			{
				StructOps->PostScriptConstruct(Struct.GetMemory());
			}
		}

		void PostLoadUObject(UObject* Object)
		{
			Object->PostLoad();
			TArray<UObject*> SubObjects;
			ForEachObjectWithOuter(Object,
				[&SubObjects](UObject* Subobject)
				{
					SubObjects.Add(Subobject);
				});

			// Cannot call PostLoad from inside ForEachObjectWithOuter, since if PostLoad calls NewObject, it will trip
			// a check that prevents creation of new objects while iterating over the UObject table.
			for (UObject* SubObject : SubObjects)
			{
				SubObject->PostLoad();
			}
		}
	}

	FFlake MakeFlake(const FName Serializer, const FConstStructView& Struct, const UObject* Outer, const FReadOptions Options)
	{
		TArray<uint8> Raw;

		if (Struct.IsValid())
		{
			if (!FFlakesModule::Get().UseSerializationProvider(Serializer,
				[&](ISerializationProvider* Provider)
				{
					Provider->Virtual_ReadData(Struct, Raw, Outer);
				}))
			{
				UE_LOG(LogFlakes, Error, TEXT("Invalid Serializer at runtime: %s"), *Serializer.ToString())
				return FFlake();
			}
		}

		FFlake Flake;
		Flake.Struct = Struct.GetScriptStruct();

#if WITH_EDITOR
		Flake.DebugString = BytesToString(Raw.GetData(), Raw.Num());
#endif

		(void)Private::CompressFlake(Flake, MoveTemp(Raw), Options);

		return Flake;
	}

	FFlake MakeFlake(const FName Serializer, const UObject* Object, const FReadOptions Options)
	{
		check(Object && !Object->IsA<AActor>());

		FFlake Flake;
		Flake.Struct = Object->GetClass();

		TArray<uint8> Raw;

		if (!FFlakesModule::Get().UseSerializationProvider(Serializer,
			[&](ISerializationProvider* Provider)
			{
				Provider->Virtual_ReadData(Object, Raw);
			}))
		{
			UE_LOG(LogFlakes, Error, TEXT("Invalid Serializer at runtime: %s"), *Serializer.ToString())
			return FFlake();
		}

#if WITH_EDITOR
		Flake.DebugString = BytesToString(Raw.GetData(), Raw.Num());
#endif

		(void)Private::CompressFlake(Flake, MoveTemp(Raw), Options);

		return Flake;
	}

	void WriteStruct(const FName Serializer, const FStructView& Struct, const FFlake& Flake, UObject* Outer, const FWriteOptions Options)
	{
		TArray<uint8> Raw;
		if (!Private::DecompressFlake(Flake, Raw, Options))
		{
			return;
		}

		if (!FFlakesModule::Get().UseSerializationProvider(Serializer,
			[&](ISerializationProvider* Provider)
			{
				Provider->Virtual_WriteData(Struct, Raw, Outer);
			}))
		{
			UE_LOG(LogFlakes, Error, TEXT("Invalid Serializer at runtime: %s"), *Serializer.ToString())
			return;
		}

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadStruct(Struct);
		}
	}

	void WriteObject(const FName Serializer, UObject* Object, const FFlake& Flake, const FWriteOptions Options)
	{
		TArray<uint8> Raw;
		if (!Private::DecompressFlake(Flake, Raw, Options))
		{
			return;
		}

		if (!FFlakesModule::Get().UseSerializationProvider(Serializer,
			[&](ISerializationProvider* Provider)
			{
				Provider->Virtual_WriteData(Object, Raw);
			}))
		{
			UE_LOG(LogFlakes, Error, TEXT("Invalid Serializer at runtime: %s"), *Serializer.ToString())
			return;
		}

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadUObject(Object);
		}
	}

	FInstancedStruct CreateStruct(const FName Serializer, const FFlake& Flake, const UScriptStruct* ExpectedStruct, const FWriteOptions Options, UObject* Outer)
	{
		UStruct* Struct = nullptr;
		if (!Private::VerifyStruct(Flake, ExpectedStruct, Struct)) return {};

		FInstancedStruct CreatedStruct;
		CreatedStruct.InitializeAs(Cast<UScriptStruct>(Struct));
		WriteStruct(Serializer, CreatedStruct, Flake, Outer, Options);

		return CreatedStruct;
	}

	UObject* CreateObject(const FName Serializer, const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass, const FWriteOptions Options)
	{
		UStruct* Struct = nullptr;
		if (!Private::VerifyStruct(Flake, ExpectedClass, Struct)) return nullptr;

		// @todo handle explicit Actor deserialization in a separate function
		/*
		if (ExpectedClass->IsChildOf<AActor>())
		{
			return nullptr;
		}
		*/

		const UClass* ObjClass = Cast<UClass>(Struct);

		// Unlikely because we already called VerifyStruct
		if (UNLIKELY(!IsValid(ObjClass)))
		{
			return nullptr;
		}

		UObject* LoadedObject = NewObject<UObject>(Outer, ObjClass);
		WriteObject(Serializer, LoadedObject, Flake, Options);

		return LoadedObject;
	}
}