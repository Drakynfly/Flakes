﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesStructs.h"
#include "FlakesMemory.h"
#include "FlakesModule.h"

#include "Compression/OodleDataCompressionUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlakesStructs)

DEFINE_LOG_CATEGORY(LogFlakes)

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
		bool VerifyStruct(const FFlake& Flake, const UStruct* Expected)
		{
			if (!ensureMsgf(IsValid(Expected), TEXT("VerifyStruct: Invalid Expected Type. Prefer passing UObject::StaticClass(), over nullptr")))
			{
				return false;
			}

			const UStruct* StructType = Cast<UStruct>(Flake.Struct.TryLoad());
			if (!IsValid(StructType))
			{
				UE_LOG(LogFlakes, Error, TEXT("VerifyStruct: Invalid Struct Type. Failed to load, or was null."));
				return false;
			}

			if (!StructType->IsChildOf(Expected))
			{
				UE_LOG(LogFlakes, Error, TEXT("VerifyStruct: StructType does match Expected type"));
				return false;
			}

			return true;
		}

		void CompressFlake(FFlake& Flake, const TArray<uint8>& Raw,
						   const FOodleDataCompression::ECompressor Compressor,
						   const FOodleDataCompression::ECompressionLevel CompressionLevel)
		{
#if WITH_EDITOR
			const auto BeforeCompression = Raw.Num();
#endif

			FOodleCompressedArray::CompressTArray(Flake.Data, Raw, Compressor, CompressionLevel);

#if WITH_EDITOR
			const auto AfterCompression = Flake.Data.Num();
			if (CVarLogCompressionStatistics.GetValueOnGameThread())
			{
				UE_LOG(LogFlakes, Log, TEXT("[Flake Compression Log]: Compressed '%i' bytes to '%i' bytes"), BeforeCompression, AfterCompression);
			}
#endif
		}

		void DecompressFlake(const FFlake& Flake, TArray<uint8>& Raw)
		{
			FOodleCompressedArray::DecompressToTArray(Raw, Flake.Data);
		}

		void PostLoadStruct(const FStructView& Struct)
		{
			check(Struct.GetScriptStruct())
			auto StructOps = Struct.GetScriptStruct()->GetCppStructOps();
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

	namespace Binary
	{
		void FSerializationProvider_Binary::ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer)
		{
			FRecursiveMemoryWriter MemoryWriter(OutData, Outer);
			// For some reason, SerializeItem is not const, so we have to const_cast the ScriptStruct
			// We also have to const_cast the memory because *we* know that this function only reads from it, but
			// SerializeItem is a bidirectional serializer, so it doesn't.
			const_cast<UScriptStruct*>(Struct.GetScriptStruct())->SerializeItem(MemoryWriter,
				const_cast<uint8*>(Struct.GetMemory()), nullptr);
			MemoryWriter.FlushCache();
			MemoryWriter.Close();
		}

		void FSerializationProvider_Binary::ReadData(const UObject* Object, TArray<uint8>& OutData)
		{
			FRecursiveMemoryWriter MemoryWriter(OutData, Object);
			const_cast<UObject*>(Object)->Serialize(MemoryWriter);
			MemoryWriter.FlushCache();
			MemoryWriter.Close();
		}

		void FSerializationProvider_Binary::WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer)
		{
			FRecursiveMemoryReader MemoryReader(Data, true, Outer);
			// For some reason, SerializeItem is not const, so we have to const_cast the ScriptStruct
			const_cast<UScriptStruct*>(Struct.GetScriptStruct())->SerializeItem(MemoryReader, Struct.GetMemory(), nullptr);
			MemoryReader.FlushCache();
			MemoryReader.Close();
		}

		void FSerializationProvider_Binary::WriteData(UObject* Object, const TArray<uint8>& Data)
		{
			FRecursiveMemoryReader MemoryReader(Data, true, Object);
			Object->Serialize(MemoryReader);
			MemoryReader.FlushCache();
			MemoryReader.Close();
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

		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

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

		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

		return Flake;
	}

	void WriteStruct(const FName Serializer, const FStructView& Struct, const FFlake& Flake, UObject* Outer, const FWriteOptions Options)
	{
		TArray<uint8> Raw;
		Private::DecompressFlake(Flake, Raw);

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
		Private::DecompressFlake(Flake, Raw);

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

	FInstancedStruct CreateStruct(const FName Serializer, const FFlake& Flake, const UScriptStruct* ExpectedStruct, UObject* Outer)
	{
		if (!Private::VerifyStruct(Flake, ExpectedStruct)) return {};

		FInstancedStruct CreatedStruct;
		CreatedStruct.InitializeAs(Cast<UScriptStruct>(Flake.Struct.TryLoad()));

		FWriteOptions Options;
		Options.ExecPostLoadOrPostScriptConstruct = true;

		WriteStruct(Serializer, CreatedStruct, Flake, Outer, Options);

		return CreatedStruct;
	}

	UObject* CreateObject(const FName Serializer, const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass)
	{
		if (!Private::VerifyStruct(Flake, ExpectedClass)) return nullptr;

		// @todo handle explicit Actor deserialization in a separate function
		/*
		if (ExpectedClass->IsChildOf<AActor>())
		{
			return nullptr;
		}
		*/

		const UClass* ObjClass = Cast<UClass>(Flake.Struct.TryLoad());

		// Unlikely because we already called VerifyStruct
		if (UNLIKELY(!IsValid(ObjClass)))
		{
			return nullptr;
		}

		UObject* LoadedObject = NewObject<UObject>(Outer, ObjClass);

		FWriteOptions Options;
		Options.ExecPostLoadOrPostScriptConstruct = true;

		WriteObject(Serializer, LoadedObject, Flake, Options);

		return LoadedObject;
	}
}