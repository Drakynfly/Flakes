// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#pragma once

namespace Flakes
{
	class FLAKES_API FRecursiveMemoryWriter : public FMemoryWriter
	{
	public:
		FRecursiveMemoryWriter(TArray<uint8>& OutBytes, const UObject* Outer)
		  : FMemoryWriter(OutBytes),
			OuterStack({Outer}) {}

		using FMemoryWriter::operator<<; // For visibility of the overloads we don't override

		//~ Begin FArchive Interface
		virtual FArchive& operator<<(UObject*& Obj) override;
		virtual FArchive& operator<<(FObjectPtr& Obj) override;
		virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override;
		virtual FArchive& operator<<(FSoftObjectPath& Value) override;
		virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
		virtual FString GetArchiveName() const override;
		//~ End FArchive Interface

	private:
		// Tracks what objects are currently being serialized. This allows us to only serialize UObjects that are directly
		// owned *and* stored in the first outer.
		TArray<const UObject*> OuterStack;
	};

	class FLAKES_API FRecursiveMemoryReader : public FMemoryReader
	{
	public:
		FRecursiveMemoryReader(const TArray<uint8>& InBytes, bool bIsPersistent, UObject* Outer)
		  : FMemoryReader(InBytes, bIsPersistent),
			OuterStack({Outer}) {}

		using FMemoryReader::operator<<; // For visibility of the overloads we don't override

		//~ Begin FArchive Interface
		virtual FArchive& operator<<(UObject*& Obj) override;
		virtual FArchive& operator<<(FObjectPtr& Obj) override;
		virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override;
		virtual FArchive& operator<<(FSoftObjectPath& Value) override;
		virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
		virtual FString GetArchiveName() const override;
		//~ End FArchive Interface

	private:
		// Tracks what objects are currently being deserialized. This allows us to reconstruct objects with their original
		// outer.
		TArray<UObject*> OuterStack;
	};
}