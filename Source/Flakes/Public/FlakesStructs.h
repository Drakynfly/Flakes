// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "Concepts/BaseStructureProvider.h"
#include "InstancedStruct.h"
#include "StructView.h"

#include "FlakesStructs.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFlakes, Log, All)

USTRUCT(BlueprintType)
struct FLAKES_API FFlake
{
	GENERATED_BODY()

	// This is either a UClass, or UScriptStruct.
	UPROPERTY()
	FSoftObjectPath Struct;

	UPROPERTY()
	TArray<uint8> Data;

	friend FArchive& operator<<(FArchive& Ar, FFlake& Flake)
	{
		Ar << Flake.Struct;
		Ar << Flake.Data;
		return Ar;
	}
};

USTRUCT()
struct FLAKES_API FFlake_Actor : public FFlake
{
	GENERATED_BODY()

	FFlake_Actor() {}

	FFlake_Actor(const FFlake& Flake)
	  : FFlake(Flake) {}

	UPROPERTY()
	FTransform Transform;

	friend FArchive& operator<<(FArchive& Ar, FFlake_Actor& Flake)
	{
		Ar << Flake.Struct;
		Ar << Flake.Data;
		Ar << Flake.Transform;
		return Ar;
	}
};

namespace Flakes
{
	struct FReadOptions
	{
		FOodleDataCompression::ECompressor Compressor = FOodleDataCompression::ECompressor::Kraken;
		FOodleDataCompression::ECompressionLevel CompressionLevel = FOodleDataCompression::ECompressionLevel::SuperFast;
	};

	struct FWriteOptions
	{
		// Calls PostLoad on the outermost UObject after deserialization, or PostScriptConstruct when deserializing structs.
		uint8 ExecPostLoadOrPostScriptConstruct : 1;
	};

	namespace Private
	{
		FLAKES_API void CompressFlake(FFlake& Flake, const TArray<uint8>& Raw,
			const FOodleDataCompression::ECompressor Compressor,
			const FOodleDataCompression::ECompressionLevel CompressionLevel);

		FLAKES_API void DecompressFlake(const FFlake& Flake, TArray<uint8>& Raw);

		FLAKES_API void PostLoadStruct(const FStructView& Struct);
		FLAKES_API void PostLoadUObject(UObject* Object);
	}

	// Low-level binary-only flake API
	FLAKES_API FFlake CreateFlake(const FConstStructView& Struct, const FReadOptions Options = {});
	FLAKES_API FFlake CreateFlake(const UObject* Object, const FReadOptions Options = {});
	FLAKES_API void WriteStruct(const FStructView& Struct, const FFlake& Flake);
	FLAKES_API void WriteObject(UObject* Object, const FFlake& Flake);
	FLAKES_API FInstancedStruct CreateStruct(const FFlake& Flake, const UScriptStruct* ExpectedStruct);
	FLAKES_API UObject* CreateObject(const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass);

	template <
		typename T
		UE_REQUIRES(TModels_V<CBaseStructureProvider, T>)
	>
	FFlake CreateFlake(const T& Struct)
	{
		return CreateFlake(FInstancedStruct::Make(Struct));
	}

	template <
		typename T
		UE_REQUIRES(TModels_V<CBaseStructureProvider, T>)
	>
	void WriteStruct(T& Struct, const FFlake& Flake)
	{
		if (Flake.Struct != TBaseStructure<T>::Get())
		{
			return;
		}

		WriteStruct(FStructView::Make(Struct), Flake);
	}

	template <typename T>
	T* CreateObject(const FFlake& Flake, UObject* Outer = GetTransientPackage())
	{
		return Cast<T>(CreateObject(Flake, Outer, T::StaticClass()));
	}

	struct ISerializationProvider : FVirtualDestructor, FNoncopyable
	{
		virtual FName GetProviderName() = 0;
		virtual void ReadData(const FConstStructView& Struct, TArray<uint8>& OutData) = 0;
		virtual void ReadData(const UObject* Object, TArray<uint8>& OutData) = 0;
		virtual void WriteData(const FStructView& Struct, const TArray<uint8>& Data) = 0;
		virtual void WriteData(UObject* Object, const TArray<uint8>& Data) = 0;
	};

	template <typename Impl>
	struct TSerializationProvider : ISerializationProvider
	{
		virtual void ReadData(const FConstStructView& Struct, TArray<uint8>& OutData) override final
		{
			Impl::Static_ReadData(Struct, OutData);
		}
		virtual void ReadData(const UObject* Object, TArray<uint8>& OutData) override final
		{
			Impl::Static_ReadData(Object, OutData);
		}
		virtual void WriteData(const FStructView& Struct, const TArray<uint8>& Data) override final
		{
			Impl::Static_WriteData(Struct, Data);
		}
		virtual void WriteData(UObject* Object, const TArray<uint8>& Data) override final
		{
			Impl::Static_WriteData(Object, Data);
		}
	};

#define SERIALIZATION_PROVIDER_HEADER(Name)\
	struct FSerializationProvider_##Name final : TSerializationProvider<FSerializationProvider_##Name>\
	{\
		virtual FName GetProviderName() override;\
		static void Static_ReadData(const FConstStructView& Struct, TArray<uint8>& OutData);\
		static void Static_ReadData(const UObject* Object, TArray<uint8>& OutData);\
		static void Static_WriteData(const FStructView& Struct, const TArray<uint8>& Data);\
		static void Static_WriteData(UObject* Object, const TArray<uint8>& Data);\
	};

	SERIALIZATION_PROVIDER_HEADER(Binary)

	// Low-level non-template flake API
	FLAKES_API FFlake CreateFlake(FName Serializer, const FConstStructView& Struct, FReadOptions Options = {});
	FLAKES_API FFlake CreateFlake(FName Serializer, const UObject* Object, FReadOptions Options = {});
	FLAKES_API void WriteStruct(FName Serializer, const FStructView& Struct, const FFlake& Flake, FWriteOptions Options = {});
	FLAKES_API void WriteObject(FName Serializer, UObject* Object, const FFlake& Flake, FWriteOptions Options = {});
	FLAKES_API FInstancedStruct CreateStruct(FName Serializer, const FFlake& Flake, const UScriptStruct* ExpectedStruct);
	FLAKES_API UObject* CreateObject(FName Serializer, const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass);

	// Low-level templated flake API
	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	static FFlake CreateFlake(const FConstStructView& Struct, const FReadOptions Options = {})
	{
		check(Struct.IsValid())

		FFlake Flake;
		Flake.Struct = Struct.GetScriptStruct();

		TArray<uint8> Raw;
		TSerializationProvider::Static_ReadData(Struct, Raw);
		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

		return Flake;
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	static FFlake CreateFlake(const UObject* Object, const FReadOptions Options = {})
	{
		check(Object && !Object->IsA<AActor>());

		FFlake Flake;
		Flake.Struct = Object->GetClass();

		TArray<uint8> Raw;
		TSerializationProvider::Static_ReadData(Object, Raw);
		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

		return Flake;
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	static void WriteStruct(const FStructView& Struct, const FFlake& Flake, const FWriteOptions Options = {})
	{
		TArray<uint8> Raw;
		Private::DecompressFlake(Flake, Raw);

		TSerializationProvider::Static_WriteData(Struct, Raw);

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadStruct(Struct);
		}
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	void WriteObject(UObject* Object, const FFlake& Flake, const FWriteOptions Options = {})
	{
		TArray<uint8> Raw;
		Private::DecompressFlake(Flake, Raw);

		TSerializationProvider::Static_WriteData(Object, Raw);

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadUObject(Object);
		}
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	FInstancedStruct CreateStruct(const FFlake& Flake, const UScriptStruct* ExpectedStruct)
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

		WriteStruct<TSerializationProvider>(CreatedStruct, Flake, Options);

		return CreatedStruct;
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	UObject* CreateObject(const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass)
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

		WriteObject<TSerializationProvider>(LoadedObject, Flake, Options);

		return LoadedObject;
	}

	template <
		typename T,
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	T* CreateObject(const FFlake& Flake, UObject* Outer = GetTransientPackage())
	{
		return Cast<T>(CreateObject<TSerializationProvider>(Flake, Outer, T::StaticClass()));
	}
}