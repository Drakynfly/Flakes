// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "Providers/FlakesBinarySerializer.h"
#include "FlakesMemory.h"

namespace Flakes::Binary
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