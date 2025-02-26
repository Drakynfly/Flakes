// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesMemory.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

namespace Flakes
{
	enum class ERecursiveMemoryObj : uint8
	{
		None,

		// Direct sub-objects are serialized with us
		Internal,

		// Any other object reference is treated as a SoftObjectRef
		External,
	};

	FArchive& FRecursiveMemoryWriter::operator<<(UObject*& Obj)
	{
		// Don't serialize nulls, or things we are already writing
		if (!IsValid(Obj) ||
			OuterStack.Contains(Obj))
		{
			return *this;
		}

		ERecursiveMemoryObj Op = ERecursiveMemoryObj::None;

		if (Obj->GetOuter() == OuterStack.Last())
		{
			Op = ERecursiveMemoryObj::Internal;
		}
		else
		{
			Op = ERecursiveMemoryObj::External;
		}

		*this << Op;

		switch (Op)
		{
		case ERecursiveMemoryObj::None:
			break;
		case ERecursiveMemoryObj::Internal:
			{
				FSoftClassPath Class(Obj->GetClass());
				*this << Class;

				OuterStack.Push(Obj); // Track that we are serializing this object
				Obj->Serialize(*this);
				OuterStack.Pop(); // Untrack the object
			}
			break;
		case ERecursiveMemoryObj::External:
			{
				FSoftObjectPath ExternalRef = Obj;
				*this << ExternalRef;
			}
			break;
		default: ;
		}

		return *this;
	}

	FArchive& FRecursiveMemoryWriter::operator<<(FObjectPtr& Obj)
	{
		if (UObject* ObjPtr = Obj.Get())
		{
			*this << ObjPtr;
		}
		return *this;
	}

	FArchive& FRecursiveMemoryWriter::operator<<(FSoftObjectPtr& AssetPtr)
	{
		return FArchiveUObject::SerializeSoftObjectPtr(*this, AssetPtr);
	}

	FArchive& FRecursiveMemoryWriter::operator<<(FSoftObjectPath& Value)
	{
		return FArchiveUObject::SerializeSoftObjectPath(*this, Value);
	}

	FArchive& FRecursiveMemoryWriter::operator<<(FWeakObjectPtr& Value)
	{
		return FArchiveUObject::SerializeWeakObjectPtr(*this, Value);
	}

	FString FRecursiveMemoryWriter::GetArchiveName() const
	{
		return TEXT("FRecursiveMemoryWriter");
	}

	FArchive& FRecursiveMemoryReader::operator<<(UObject*& Obj)
	{
		ERecursiveMemoryObj Op;
		*this << Op;

		switch (Op)
		{
		case ERecursiveMemoryObj::Internal:
			{
				FSoftClassPath Class;
				*this << Class;

				if (const UClass* ObjClass = Class.TryLoadClass<UObject>())
				{
					Obj = NewObject<UObject>(OuterStack.Last(), ObjClass);
					OuterStack.Push(Obj);
					Obj->Serialize(*this);
					OuterStack.Pop();
				}
			}
			break;
		case ERecursiveMemoryObj::External:
			{
				FSoftObjectPath ExternalRef;
				*this << ExternalRef;
				Obj = ExternalRef.TryLoad();
			}
			break;
		case ERecursiveMemoryObj::None:
		default: return *this;
		}

		return *this;
	}

	FArchive& FRecursiveMemoryReader::operator<<(FObjectPtr& Obj)
	{
		UObject* RawObj = nullptr;
		*this << RawObj;
		Obj = RawObj;
		return *this;
	}

	FArchive& FRecursiveMemoryReader::operator<<(FSoftObjectPtr& AssetPtr)
	{
		return FArchiveUObject::SerializeSoftObjectPtr(*this, AssetPtr);
	}

	FArchive& FRecursiveMemoryReader::operator<<(FSoftObjectPath& Value)
	{
		return FArchiveUObject::SerializeSoftObjectPath(*this, Value);
	}

	FArchive& FRecursiveMemoryReader::operator<<(FWeakObjectPtr& Value)
	{
		return FArchiveUObject::SerializeWeakObjectPtr(*this, Value);
	}

	FString FRecursiveMemoryReader::GetArchiveName() const
	{
		return TEXT("FRecursiveMemoryReader");
	}
}