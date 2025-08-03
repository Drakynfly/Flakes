// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

#include "Compression/OodleDataCompression.h"
#include "Concepts/BaseStructureProvider.h"
#include "FlakesData.h"
#include "GameFramework/Actor.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "UObject/Package.h"

namespace Flakes
{
	struct FReadOptions
	{
		FOodleDataCompression::ECompressor Compressor = FOodleDataCompression::ECompressor::Kraken;
		FOodleDataCompression::ECompressionLevel CompressionLevel = FOodleDataCompression::ECompressionLevel::SuperFast;
	};

	struct FWriteOptions
	{
		// Skips running the DecompressFlake step. Only safe to enable if you know the source is not compressed.
		uint8 SkipDecompressionStep : 1 = false;

		// Calls PostLoad on the outermost UObject after deserialization, or PostScriptConstruct when deserializing structs.
		uint8 ExecPostLoadOrPostScriptConstruct : 1 = false;
	};

	// This is the default value for CreateX functions as they have PostLoad/Construct enabled by default for back-compat.
	inline static FWriteOptions CreationDefault{
		.ExecPostLoadOrPostScriptConstruct = true,
	};

	namespace Private
	{
		[[nodiscard]] FLAKES_API bool VerifyStruct(const FFlake& Flake, const UStruct* Expected, UStruct*& OutStruct);

		[[nodiscard]] FLAKES_API bool CompressFlake(FFlake& Flake, TArray<uint8>&& Raw, const FReadOptions& Options);
		[[nodiscard]] FLAKES_API bool DecompressFlake(const FFlake& Flake, TArray<uint8>& Raw, const FWriteOptions& Options);

		FLAKES_API void PostLoadStruct(const FStructView& Struct);
		FLAKES_API void PostLoadUObject(UObject* Object);
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
#define SERIALIZATION_PROVIDER_HEADER(API, Name, Pseudonym)\
	struct API FSerializationProvider_##Name final : TSerializationProvider<FSerializationProvider_##Name>\
	{\
		static inline const FLazyName ProviderName = FLazyName(TEXT(#Name));\
		virtual FName GetProviderName() override\
		{\
			return ProviderName;\
		}\
		static void ReadData(const FConstStructView& Struct, TArray<uint8>& OutData, const UObject* Outer = nullptr);\
		static void ReadData(const UObject* Object, TArray<uint8>& OutData);\
		static void WriteData(const FStructView& Struct, const TArray<uint8>& Data, UObject* Outer = nullptr);\
		static void WriteData(UObject* Object, const TArray<uint8>& Data);\
	};\
	using Pseudonym = FSerializationProvider_##Name;

	// Low-level non-template flake API
	FLAKES_API FFlake MakeFlake(FName Serializer, const FConstStructView& Struct, const UObject* Outer = nullptr, FReadOptions Options = {});
	FLAKES_API FFlake MakeFlake(FName Serializer, const UObject* Object, FReadOptions Options = {});
	FLAKES_API void WriteStruct(FName Serializer, const FStructView& Struct, const FFlake& Flake, UObject* Outer = nullptr, FWriteOptions Options = {});
	FLAKES_API void WriteObject(FName Serializer, UObject* Object, const FFlake& Flake, FWriteOptions Options = {});
	FLAKES_API FInstancedStruct CreateStruct(FName Serializer, const FFlake& Flake, const UScriptStruct* ExpectedStruct, FWriteOptions Options = CreationDefault, UObject* Outer = nullptr);
	FLAKES_API UObject* CreateObject(FName Serializer, const FFlake& Flake, UObject* Outer, const UClass* ExpectedClass, FWriteOptions Options = CreationDefault);

	template <
		typename T
	>
	T* CreateObject(FName Serializer, const FFlake& Flake, UObject* Outer = GetTransientPackage(), FWriteOptions Options = CreationDefault)
	{
		return Cast<T>(CreateObject(Serializer, Flake, Outer, T::StaticClass(), Options));
	}

	// Low-level templated flake API
	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	FFlake MakeFlake(const FConstStructView& Struct, const UObject* Outer, const FReadOptions Options = {})
	{
		check(Struct.IsValid())

		FFlake Flake;
		Flake.Struct = Struct.GetScriptStruct();

		TArray<uint8> Raw;
		TSerializationProvider::ReadData(Struct, Raw, Outer);

#if WITH_EDITOR
		Flake.DebugString = BytesToString(Raw.GetData(), Raw.Num());
#endif

		(void)Private::CompressFlake(Flake, MoveTemp(Raw), Options);

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

#if WITH_EDITOR
		Flake.DebugString = BytesToString(Raw.GetData(), Raw.Num());
#endif

		(void)Private::CompressFlake(Flake, MoveTemp(Raw), Options);

		return Flake;
	}

	template <
		typename TSerializationProvider,
		typename TStruct
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	FFlake MakeFlake(const TStruct& Struct, const UObject* Outer, const FReadOptions Options = {})
	{
		return MakeFlake<TSerializationProvider>(FConstStructView::Make(Struct), Outer, Options);
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	void WriteStruct(const FStructView& Struct, const FFlake& Flake, UObject* Outer, const FWriteOptions Options = {})
	{
		TArray<uint8> Raw;
		if (!Private::DecompressFlake(Flake, Raw, Options))
		{
			return;
		}

		TSerializationProvider::WriteData(Struct, Raw, Outer);

		if (Options.ExecPostLoadOrPostScriptConstruct)
		{
			Private::PostLoadStruct(Struct);
		}
	}

	template <
		typename TSerializationProvider,
		typename TStruct
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	void WriteStruct(TStruct& Struct, const FFlake& Flake, UObject* Outer, const FWriteOptions Options = {})
	{
		WriteStruct<TSerializationProvider>(FStructView::Make(Struct), Flake, Outer, Options);
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	void WriteObject(UObject* Object, const FFlake& Flake, const FWriteOptions Options = {})
	{
		TArray<uint8> Raw;
		if (!Private::DecompressFlake(Flake, Raw, Options))
		{
			return;
		}

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
	FInstancedStruct CreateStruct(const FFlake& Flake, const UScriptStruct* ExpectedStruct, const FWriteOptions Options = CreationDefault, UObject* Outer = nullptr)
	{
		UStruct* Struct = nullptr;
		if (!Private::VerifyStruct(Flake, ExpectedStruct, Struct)) return {};

		FInstancedStruct CreatedStruct;
		CreatedStruct.InitializeAs(Cast<UScriptStruct>(Struct));

		WriteStruct<TSerializationProvider>(CreatedStruct, Flake, Outer, Options);

		return CreatedStruct;
	}

	template <
		typename TSerializationProvider,
		typename TStruct
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value &&
					TModels_V<CBaseStructureProvider, TStruct>)
	>
	TStruct CreateStruct(const FFlake& Flake, UObject* Outer = nullptr, const FWriteOptions Options = CreationDefault)
	{
		UStruct* Struct = nullptr;
		if (!Private::VerifyStruct(Flake, TBaseStructure<TStruct>::Get(), Struct)) return {};

		TStruct CreatedStruct;
		WriteStruct<TSerializationProvider>(FStructView::Make(CreatedStruct), Flake, Outer, Options);

		return CreatedStruct;
	}

	template <
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	UObject* CreateObject(const FFlake& Flake, const UClass* ExpectedClass, UObject* Outer = GetTransientPackage(), const FWriteOptions Options = CreationDefault)
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
		WriteObject<TSerializationProvider>(LoadedObject, Flake, Options);

		return LoadedObject;
	}

	template <
		typename T,
		typename TSerializationProvider
		UE_REQUIRES(TIsDerivedFrom<TSerializationProvider, ISerializationProvider>::Value)
	>
	T* CreateObject(const FFlake& Flake, UObject* Outer = GetTransientPackage(), FWriteOptions Options = CreationDefault)
	{
		return Cast<T>(CreateObject<TSerializationProvider>(Flake, T::StaticClass(), Outer, Options));
	}
}