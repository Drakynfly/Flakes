// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

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

	FFlake CreateFlake(const FConstStructView& Struct, const FReadOptions Options)
	{
		return CreateFlake<FSerializationProvider_Binary>(Struct, Options);
	}

	FFlake CreateFlake(const UObject* Object, const FReadOptions Options)
	{
		return CreateFlake<FSerializationProvider_Binary>(Object, Options);
	}

	void WriteStruct(const FStructView& Struct, const FFlake& Flake)
	{
		WriteStruct<FSerializationProvider_Binary>(Struct, Flake);
	}

	void WriteObject(UObject* Object, const FFlake& Flake)
	{
		WriteObject<FSerializationProvider_Binary>(Object, Flake);
	}

	FInstancedStruct CreateStruct(const FFlake& Flake, const UScriptStruct* ExpectedStruct)
	{
		return CreateStruct<FSerializationProvider_Binary>(Flake, ExpectedStruct);
	}

	UObject* CreateObject(const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass)
	{
		return CreateObject<FSerializationProvider_Binary>(Flake, Outer, ExpectedClass);
	}

	void FSerializationProvider_Binary::ReadData(const FConstStructView& Struct, TArray<uint8>& OutData)
	{
		FRecursiveMemoryWriter MemoryWriter(OutData, nullptr);
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

	void FSerializationProvider_Binary::WriteData(const FStructView& Struct, const TArray<uint8>& Data)
	{
		FRecursiveMemoryReader MemoryReader(Data, true, nullptr);
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

	FFlake CreateFlake(const FName Serializer, const FConstStructView& Struct, const FReadOptions Options)
	{
		TArray<uint8> Raw;

		if (Struct.IsValid())
		{
			FFlakesModule::Get().UseSerializationProvider(Serializer,
				[&](ISerializationProvider* Provider)
				{
					Provider->Virtual_ReadData(Struct, Raw);
				});
		}

		FFlake Flake;
		Flake.Struct = Struct.GetScriptStruct();

		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

		return Flake;
	}

	FFlake CreateFlake(const FName Serializer, const UObject* Object, const FReadOptions Options)
	{
		check(Object && !Object->IsA<AActor>());

		FFlake Flake;
		Flake.Struct = Object->GetClass();

		TArray<uint8> Raw;

		FFlakesModule::Get().UseSerializationProvider(Serializer,
			[&](ISerializationProvider* Provider)
			{
				Provider->Virtual_ReadData(Object, Raw);
			});

		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

		return Flake;
	}

	void WriteStruct(const FName Serializer, const FStructView& Struct, const FFlake& Flake, const FWriteOptions Options)
	{
		TArray<uint8> Raw;
		Private::DecompressFlake(Flake, Raw);

		FFlakesModule::Get().UseSerializationProvider(Serializer,
			[&](ISerializationProvider* Provider)
			{
				Provider->Virtual_WriteData(Struct, Raw);
			});

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadStruct(Struct);
		}
	}

	void WriteObject(const FName Serializer, UObject* Object, const FFlake& Flake, const FWriteOptions Options)
	{
		TArray<uint8> Raw;
		Private::DecompressFlake(Flake, Raw);

		FFlakesModule::Get().UseSerializationProvider(Serializer,
			[&](ISerializationProvider* Provider)
			{
				Provider->Virtual_WriteData(Object, Raw);
			});

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadUObject(Object);
		}
	}

	FInstancedStruct CreateStruct(const FName Serializer, const FFlake& Flake, const UScriptStruct* ExpectedStruct)
	{
		if (!IsValid(ExpectedStruct))
		{
			return {};
		}

		const UScriptStruct* StructType = Cast<UScriptStruct>(Flake.Struct.TryLoad());

		if (!IsValid(StructType))
		{
			return {};
		}

		if (!StructType->IsChildOf(ExpectedStruct))
		{
			return {};
		}

		FInstancedStruct CreatedStruct;
		CreatedStruct.InitializeAs(StructType);

		FWriteOptions Options;
		Options.ExecPostLoadOrPostScriptConstruct = true;

		WriteStruct(Serializer, CreatedStruct, Flake, Options);

		return CreatedStruct;
	}

	UObject* CreateObject(const FName Serializer, const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass)
	{
		if (!IsValid(ExpectedClass) || ExpectedClass->IsChildOf<AActor>())
		{
			return nullptr;
		}

		const UClass* ObjClass = LoadClass<UObject>(nullptr, *Flake.Struct.ToString(), nullptr, LOAD_None, nullptr);

		if (!IsValid(ObjClass))
		{
			return nullptr;
		}

		if (!ObjClass->IsChildOf(ExpectedClass))
		{
			return nullptr;
		}

		auto&& LoadedObject = NewObject<UObject>(Outer, ObjClass);

		FWriteOptions Options;
		Options.ExecPostLoadOrPostScriptConstruct = true;

		WriteObject(Serializer, LoadedObject, Flake, Options);

		return LoadedObject;
	}
}