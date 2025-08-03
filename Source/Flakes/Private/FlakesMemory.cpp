// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "FlakesMemory.h"
#include "FlakesLogging.h"
#include "Engine/World.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

namespace Flakes
{
	enum class ERecursiveMemoryObj : uint8
	{
		None,

		// Direct sub-objects are serialized with us
		Exported,

		// Any other object reference is treated as a SoftObjectRef
		Reference,
	};

	FArchive& FRecursiveMemoryWriter::operator<<(UObject*& Obj)
	{
		// Don't serialize nulls
		if (!IsValid(Obj))
		{
			return *this;
		}

		ERecursiveMemoryObj Op = ERecursiveMemoryObj::None;

		/*
		 * Conditions for being exports are either:
		 * 1. Being owned by something that is already being exported, or
		 * 2. Being owned by a world, and therefor is not an asset from disk.
		 */
		const bool ShouldExport =
			OuterStack.Contains(Obj->GetOuter()) ||
			Obj->GetTypedOuter<UWorld>();

		// If this object is already being exported, then prevent additional exports.
		const bool CannotExport = ExportedObjects.Contains(Obj);

		if (ShouldExport && !CannotExport)
		{
			Op = ERecursiveMemoryObj::Exported;
		}
		else
		{
			Op = ERecursiveMemoryObj::Reference;
		}

		*this << Op;

		switch (Op)
		{
		case ERecursiveMemoryObj::None:
			break;
		case ERecursiveMemoryObj::Exported:
			{
				FSoftClassPath Class(Obj->GetClass());
				*this << Class;
				/*
				FString ObjectName(Obj->GetName());
				*this << ObjectName;
				*/

				// Track this export, so we do not export twice.
				ExportedObjects.Push(Obj);

				OuterStack.Push(Obj); // Track that we are serializing this object
				Obj->Serialize(*this);
				OuterStack.Pop(); // Untrack the object
			}
			break;
		case ERecursiveMemoryObj::Reference:
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
		case ERecursiveMemoryObj::Exported:
			{
				FSoftClassPath Class;
				*this << Class;
				/*
				FName ObjectName;
				*this << ObjectName;
				*/

				if (Class.IsNull())
				{
					UE_LOG(LogFlakes, Error, TEXT("FRecursiveMemoryReader failed to load Class: Null Path"))
					SetError();
				}
				else if (const UClass* ObjClass = Class.TryLoadClass<UObject>())
				{
					Obj = NewObject<UObject>(OuterStack.Last(), ObjClass/*, ObjectName*/);
					OuterStack.Push(Obj);
					Obj->Serialize(*this);
					OuterStack.Pop();
				}
				else
				{
					UE_LOG(LogFlakes, Error, TEXT("FRecursiveMemoryReader failed to load Class: '%s'"), *Class.ToString())
					SetError();
				}
			}
			break;
		case ERecursiveMemoryObj::Reference:
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