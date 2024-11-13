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
		FLAKES_API bool VerifyStruct(const FFlake& Flake, const UStruct* Expected);

		FLAKES_API void CompressFlake(FFlake& Flake, const TArray<uint8>& Raw,
			const FOodleDataCompression::ECompressor Compressor,
			const FOodleDataCompression::ECompressionLevel CompressionLevel);

		FLAKES_API void DecompressFlake(const FFlake& Flake, TArray<uint8>& Raw);

		FLAKES_API void PostLoadStruct(const FStructView& Struct);
		FLAKES_API void PostLoadUObject(UObject* Object);
	}

	// Low-level binary-only flake API
	FLAKES_API FFlake MakeFlake(const FConstStructView& Struct, const UObject* Outer, const FReadOptions Options = {});
	FLAKES_API FFlake MakeFlake(const UObject* Object, const FReadOptions Options = {});
	FLAKES_API void WriteStruct(const FStructView& Struct, const FFlake& Flake, UObject* Outer = nullptr);
	FLAKES_API void WriteObject(UObject* Object, const FFlake& Flake);
	FLAKES_API FInstancedStruct CreateStruct(const FFlake& Flake, const UScriptStruct* ExpectedStruct, UObject* Outer = nullptr);
	FLAKES_API UObject* CreateObject(const FFlake& Flake, const UClass* ExpectedClass, UObject* Outer = nullptr);

	template <
		typename T
		UE_REQUIRES(TModels_V<CBaseStructureProvider, T>)
	>
	FFlake MakeFlake(const T& Struct)
	{
		return MakeFlake(FConstStructView::Make(Struct));
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
	T CreateStruct(const FFlake& Flake, UObject* Outer = nullptr)
	{
		return CreateStruct(Flake, T::StaticStruct(), Outer).template Get<T>();
	}

	template <typename T>
	T* CreateObject(const FFlake& Flake, UObject* Outer = GetTransientPackage())
	{
		return Cast<T>(CreateObject(Flake, T::StaticClass(), Outer));
	}

	/* Interface for using Providers dynamically from their FName. */
	struct ISerializationProvider : FVirtualDestructor, FNoncopyable
	{
		virtual FName GetProviderName() = 0;
		virtual void Virtual_ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer = nullptr) = 0;
		virtual void Virtual_ReadData(const UObject* Object, TArray<uint8>& OutData) = 0;
		virtual void Virtual_WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer = nullptr) = 0;
		virtual void Virtual_WriteData(UObject* Object, const TArray<uint8>& Data) = 0;
	};

	// Template implementation of ISerializationProvider that forwards to the static versions.
	template <typename Impl>
	struct TSerializationProvider : ISerializationProvider
	{
		virtual void Virtual_ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer = nullptr) override final
		{
			Impl::ReadData(Struct, OutData, Outer);
		}
		virtual void Virtual_ReadData(const UObject* Object, TArray<uint8>& OutData) override final
		{
			Impl::ReadData(Object, OutData);
		}
		virtual void Virtual_WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer = nullptr) override final
		{
			Impl::WriteData(Struct, Data, Outer);
		}
		virtual void Virtual_WriteData(UObject* Object, const TArray<uint8>& Data) override final
		{
			Impl::WriteData(Object, Data);
		}
	};

	// Macro to declare a new provider. The implementations of ReadData and WriteData must be defined to match these signatures.
#define SERIALIZATION_PROVIDER_HEADER(API, Name)\
	struct API FSerializationProvider_##Name final : TSerializationProvider<FSerializationProvider_##Name>\
	{\
		virtual FName GetProviderName() override\
		{\
			static const FLazyName Name##SerializationProvider(TEXT(#Name));\
			return Name##SerializationProvider;\
		}\
		static void ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer = nullptr);\
		static void ReadData(const UObject* Object, TArray<uint8>& OutData);\
		static void WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer = nullptr);\
		static void WriteData(UObject* Object, const TArray<uint8>& Data);\
	};\
	using Type = FSerializationProvider_##Name;

	namespace Binary
	{
		SERIALIZATION_PROVIDER_HEADER(FLAKES_API, Binary)
	}

	// Low-level non-template flake API
	FLAKES_API FFlake MakeFlake(FName Serializer, const FConstStructView& Struct, const UObject* Outer = nullptr, FReadOptions Options = {});
	FLAKES_API FFlake MakeFlake(FName Serializer, const UObject* Object, FReadOptions Options = {});
	FLAKES_API void WriteStruct(FName Serializer, const FStructView& Struct, const FFlake& Flake, UObject* Outer = nullptr, FWriteOptions Options = {});
	FLAKES_API void WriteObject(FName Serializer, UObject* Object, const FFlake& Flake, FWriteOptions Options = {});
	FLAKES_API FInstancedStruct CreateStruct(FName Serializer, const FFlake& Flake, const UScriptStruct* ExpectedStruct, UObject* Outer = nullptr);
	FLAKES_API UObject* CreateObject(FName Serializer, const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass);

	// Low-level templated flake API
	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	FFlake MakeFlake(const FConstStructView& Struct, const UObject* Outer = nullptr, const FReadOptions Options = {})
	{
		check(Struct.IsValid())

		FFlake Flake;
		Flake.Struct = Struct.GetScriptStruct();

		TArray<uint8> Raw;
		TSerializationProvider::ReadData(Struct, Raw, Outer);
		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

		return Flake;
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	FFlake MakeFlake(const UObject* Object, const FReadOptions Options = {})
	{
		check(Object && !Object->IsA<AActor>());

		FFlake Flake;
		Flake.Struct = Object->GetClass();

		TArray<uint8> Raw;
		TSerializationProvider::ReadData(Object, Raw);
		Private::CompressFlake(Flake, Raw, Options.Compressor, Options.CompressionLevel);

		return Flake;
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	void WriteStruct(const FStructView& Struct, const FFlake& Flake, UObject* Outer = nullptr, const FWriteOptions Options = {})
	{
		TArray<uint8> Raw;
		Private::DecompressFlake(Flake, Raw);

		TSerializationProvider::WriteData(Struct, Raw, Outer);

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

		TSerializationProvider::WriteData(Object, Raw);

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadUObject(Object);
		}
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	FInstancedStruct CreateStruct(const FFlake& Flake, const UScriptStruct* ExpectedStruct, UObject* Outer = nullptr)
	{
		if (!Private::VerifyStruct(Flake, ExpectedStruct)) return {};

		FInstancedStruct CreatedStruct;
		CreatedStruct.InitializeAs(Cast<UScriptStruct>(Flake.Struct.TryLoad()));

		FWriteOptions Options;
		Options.ExecPostLoadOrPostScriptConstruct = true;

		WriteStruct<TSerializationProvider>(CreatedStruct, Flake, Outer, Options);

		return CreatedStruct;
	}

	template <
		typename TSerializationProvider,
		typename TStruct
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value &&
					TModels_V<CBaseStructureProvider, TStruct>)
	>
	TStruct CreateStruct(const FFlake& Flake, UObject* Outer = nullptr)
	{
		if (!Private::VerifyStruct(Flake, TBaseStructure<TStruct>::Get())) return {};

		TStruct CreatedStruct;

		FWriteOptions Options;
		Options.ExecPostLoadOrPostScriptConstruct = true;

		WriteStruct<TSerializationProvider>(FStructView::Make(CreatedStruct), Flake, Outer, Options);

		return CreatedStruct;
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	UObject* CreateObject(const FFlake& Flake, const UClass* ExpectedClass, UObject* Outer = GetTransientPackage())
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
		return Cast<T>(CreateObject<TSerializationProvider>(Flake, T::StaticClass(), Outer));
	}
}