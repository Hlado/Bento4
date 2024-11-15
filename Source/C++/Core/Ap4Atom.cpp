/*****************************************************************
|
|    AP4 - Atoms
|
|    Copyright 2002-2008 Axiomatic Systems, LLC
|
|
|    This file is part of Bento4/AP4 (MP4 Atom Processing Library).
|
|    Unless you have obtained Bento4 under a difference license,
|    this version of Bento4 is Bento4|GPL.
|    Bento4|GPL is free software; you can redistribute it and/or modify
|    it under the terms of the GNU General Public License as published by
|    the Free Software Foundation; either version 2, or (at your option)
|    any later version.
|
|    Bento4|GPL is distributed in the hope that it will be useful,
|    but WITHOUT ANY WARRANTY; without even the implied warranty of
|    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|    GNU General Public License for more details.
|
|    You should have received a copy of the GNU General Public License
|    along with Bento4|GPL; see the file COPYING.  If not, write to the
|    Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
|    02111-1307, USA.
|
 ****************************************************************/

//Modified by github user @Hlado 06/27/2024

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "Ap4Types.h"
#include "Ap4Atom.h"
#include "Ap4Utils.h"
#include "Ap4ContainerAtom.h"
#include "Ap4AtomFactory.h"
#include "Ap4Debug.h"
#include "Ap4UuidAtom.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
static const unsigned int AP4_ATOM_MAX_CLONE_SIZE = 1048576; // 1 meg
static const unsigned int AP4_UNKNOWN_ATOM_MAX_LOCAL_PAYLOAD_SIZE = 4096;

/*----------------------------------------------------------------------
|   dynamic cast support
+---------------------------------------------------------------------*/
AP4_DEFINE_DYNAMIC_CAST_ANCHOR(AP4_Atom)
AP4_DEFINE_DYNAMIC_CAST_ANCHOR(AP4_AtomParent)
AP4_DEFINE_DYNAMIC_CAST_ANCHOR(AP4_NullTerminatedStringAtom)

/*----------------------------------------------------------------------
|   AP4_Atom::TypeFromString
+---------------------------------------------------------------------*/
AP4_Atom::Type
AP4_Atom::TypeFromString(const char* s)
{
    // convert the name into an atom type
    return ((AP4_UI32)s[0])<<24 |
           ((AP4_UI32)s[1])<<16 |
           ((AP4_UI32)s[2])<< 8 |
           ((AP4_UI32)s[3]);
}

/*----------------------------------------------------------------------
|   AP4_Atom::AP4_Atom
+---------------------------------------------------------------------*/
AP4_Atom::AP4_Atom(Type type, AP4_UI32 size /* = AP4_ATOM_HEADER_SIZE */) :
    m_Type(type),
    m_Size32(size),
    m_Size64(0),
    m_IsFull(false),
    m_Version(0),
    m_Flags(0),
    m_Parent(NULL)
{
}

/*----------------------------------------------------------------------
|   AP4_Atom::AP4_Atom
+---------------------------------------------------------------------*/
AP4_Atom::AP4_Atom(Type type, AP4_UI64 size, bool force_64) :
    m_Type(type),
    m_Size32(0),
    m_Size64(0),
    m_IsFull(false),
    m_Version(0),
    m_Flags(0),
    m_Parent(NULL)
{
    SetSize(size, force_64);
}

/*----------------------------------------------------------------------
|   AP4_Atom::AP4_Atom
+---------------------------------------------------------------------*/
AP4_Atom::AP4_Atom(Type     type,
                   AP4_UI32 size,
                   AP4_UI08 version,
                   AP4_UI32 flags) :
    m_Type(type),
    m_Size32(size),
    m_Size64(0),
    m_IsFull(true),
    m_Version(version),
    m_Flags(flags),
    m_Parent(NULL)
{
}

/*----------------------------------------------------------------------
|   AP4_Atom::AP4_Atom
+---------------------------------------------------------------------*/
AP4_Atom::AP4_Atom(Type     type,
                   AP4_UI64 size,
                   bool     force_64,
                   AP4_UI08 version,
                   AP4_UI32 flags) :
    m_Type(type),
    m_Size32(0),
    m_Size64(0),
    m_IsFull(true),
    m_Version(version),
    m_Flags(flags),
    m_Parent(NULL)
{
    SetSize(size, force_64);
}

/*----------------------------------------------------------------------
|   AP4_Atom::ReadFullHeader
+---------------------------------------------------------------------*/
AP4_Result
AP4_Atom::ReadFullHeader(AP4_ByteStream& stream,
                         AP4_UI08&       version,
                         AP4_UI32&       flags)
{
    AP4_UI32 header;
    AP4_CHECK(stream.ReadUI32(header));
    version = (header>>24)&0x000000FF;
    flags   = (header    )&0x00FFFFFF;

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_Atom::SetSize
+---------------------------------------------------------------------*/
void
AP4_Atom::SetSize(AP4_UI64 size, bool force_64)
{
    if (!force_64) {
        // see if we need to implicitely force 64-bit encoding
        if (m_Size32 == 1 && m_Size64 <= 0xFFFFFFFF) {
            // we have a forced 64-bit encoding
            force_64 = true;
        }
    }
    if ((size >> 32) == 0 && !force_64) {
        m_Size32 = (AP4_UI32)size;
        m_Size64 = 0;
    } else {
        m_Size32 = 1;
        m_Size64 = size;
    }
}

/*----------------------------------------------------------------------
|   AP4_Atom::GetHeaderSize
+---------------------------------------------------------------------*/
AP4_Size
AP4_Atom::GetHeaderSize() const
{
    return (m_IsFull ? AP4_FULL_ATOM_HEADER_SIZE : AP4_ATOM_HEADER_SIZE)+(m_Size32==1?8:0);
}

/*----------------------------------------------------------------------
|   AP4_Atom::WriteHeader
+---------------------------------------------------------------------*/
AP4_Result
AP4_Atom::WriteHeader(AP4_ByteStream& stream)
{
    AP4_Result result;

    // write the size
    result = stream.WriteUI32(m_Size32);
    if (AP4_FAILED(result)) return result;

    // write the type
    result = stream.WriteUI32(m_Type);
    if (AP4_FAILED(result)) return result;

    // handle 64-bit sizes
    if (m_Size32 == 1) {
        result = stream.WriteUI64(m_Size64);
        if (AP4_FAILED(result)) return result;
    }

    // for full atoms, write version and flags
    if (m_IsFull) {
        result = stream.WriteUI08(m_Version);
        if (AP4_FAILED(result)) return result;
        result = stream.WriteUI24(m_Flags);
        if (AP4_FAILED(result)) return result;
    }

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_Atom::Write
+---------------------------------------------------------------------*/
AP4_Result
AP4_Atom::Write(AP4_ByteStream& stream)
{
    AP4_Result result;

#if defined(AP4_DEBUG)
    AP4_Position before;
    stream.Tell(before);
#endif

    // write the header
    result = WriteHeader(stream);
    if (AP4_FAILED(result)) return result;

    // write the fields
    result = WriteFields(stream);
    if (AP4_FAILED(result)) return result;

#if defined(AP4_DEBUG)
    AP4_Position after;
    stream.Tell(after);
    AP4_UI64 atom_size = GetSize();
    if (after-before != atom_size) {
        AP4_Debug("ERROR: atom size mismatch (declared size=%d, actual size=%d)\n",
                  (AP4_UI32)atom_size, (AP4_UI32)(after-before));
        AP4_Atom* atom = this;
        while (atom) {
            char name[7];
            name[0] = '[';
            AP4_FormatFourCharsPrintable(&name[1], atom->GetType());
            name[5] = ']';
            name[6] = '\0';
            AP4_Debug("       while writing %s\n", name);
            atom = AP4_DYNAMIC_CAST(AP4_Atom, atom->GetParent());
        }
        AP4_ASSERT(after-before == atom_size);
    }
#endif

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_Atom::Inspect
+---------------------------------------------------------------------*/
AP4_Result
AP4_Atom::Inspect(AP4_AtomInspector& inspector)
{
    InspectHeader(inspector);
    InspectFields(inspector);
    inspector.EndAtom();

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_Atom::InspectHeader
+---------------------------------------------------------------------*/
AP4_Result
AP4_Atom::InspectHeader(AP4_AtomInspector& inspector)
{
    char name[5];
    AP4_FormatFourCharsPrintable(name, m_Type);
    name[4] = '\0';
    inspector.StartAtom(name,
                        m_Version,
                        m_Flags,
                        GetHeaderSize(),
                        GetSize());

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_Atom::Detach
+---------------------------------------------------------------------*/
AP4_Result
AP4_Atom::Detach()
{
    if (m_Parent) {
        return m_Parent->RemoveChild(this);
    } else {
        return AP4_SUCCESS;
    }
}

/*----------------------------------------------------------------------
|   AP4_Atom::Clone
+---------------------------------------------------------------------*/
AP4_Atom*
AP4_Atom::Clone()
{
    AP4_Atom* clone = NULL;

    // check the size (refuse to clone atoms that are too large)
    AP4_LargeSize size = GetSize();
    if (size > AP4_ATOM_MAX_CLONE_SIZE) return NULL;

    // create a memory byte stream to which we can serialize
    auto mbs = std::make_shared<AP4_MemoryByteStream>((AP4_Size)GetSize());

    // serialize to memory
    if (AP4_FAILED(Write(*mbs))) {
        return NULL;
    }

    // create the clone from the serialized form
    mbs->Seek(0);
    AP4_DefaultAtomFactory atom_factory;
    atom_factory.CreateAtomFromStream(mbs, clone);

    return clone;
}

/*----------------------------------------------------------------------
|   AP4_UnknownAtom::AP4_UnknownAtom
+---------------------------------------------------------------------*/
AP4_UnknownAtom::AP4_UnknownAtom(Type            type,
                                 AP4_UI64        size,
                                 std::shared_ptr<AP4_ByteStream> stream) :
    AP4_Atom(type, size),
    //no move here because of original function logic using argument after setting member to null
    m_SourceStream(stream)
{
    if (size <= AP4_UNKNOWN_ATOM_MAX_LOCAL_PAYLOAD_SIZE &&
        type != AP4_ATOM_TYPE_MDAT) {
        m_SourcePosition = 0;
        m_SourceStream.reset();
        AP4_UI32 payload_size = (AP4_UI32)size-GetHeaderSize();
        m_Payload.SetDataSize(payload_size);
        stream->Read(m_Payload.UseData(), payload_size);
        return;
    }

    // store source stream position
    stream->Tell(m_SourcePosition);

    // clamp to the file size
    AP4_UI64 file_size;
    if (AP4_SUCCEEDED(stream->GetSize(file_size))) {
        if (m_SourcePosition-GetHeaderSize()+size > file_size) {
            if (m_Size32 == 1) {
                // size is encoded as a large size
                m_Size64 = file_size-m_SourcePosition;
            } else {
                AP4_ASSERT(size <= 0xFFFFFFFF);
                m_Size32 = (AP4_UI32)(file_size-m_SourcePosition);
            }
        }
    }
}

/*----------------------------------------------------------------------
|   AP4_UnknownAtom::AP4_UnknownAtom
+---------------------------------------------------------------------*/
AP4_UnknownAtom::AP4_UnknownAtom(Type            type,
                                 const AP4_UI08* payload,
                                 AP4_Size        payload_size) :
    AP4_Atom(type, AP4_ATOM_HEADER_SIZE+payload_size, false),
    m_SourceStream(nullptr),
    m_SourcePosition(0)
{
    m_Payload.SetData(payload, payload_size);
}

/*----------------------------------------------------------------------
|   AP4_UnknownAtom::AP4_UnknownAtom
+---------------------------------------------------------------------*/
AP4_UnknownAtom::AP4_UnknownAtom(const AP4_UnknownAtom& other) :
    AP4_Atom(other.m_Type, (AP4_UI32)0),
    m_SourceStream(other.m_SourceStream),
    m_SourcePosition(other.m_SourcePosition),
    m_Payload(other.m_Payload)
{
    m_Size32 = other.m_Size32;
    m_Size64 = other.m_Size64;
}

/*----------------------------------------------------------------------
|   AP4_UnknownAtom::~AP4_UnknownAtom
+---------------------------------------------------------------------*/
AP4_UnknownAtom::~AP4_UnknownAtom()
{

}

/*----------------------------------------------------------------------
|   AP4_UnknownAtom::WriteFields
+---------------------------------------------------------------------*/
AP4_Result
AP4_UnknownAtom::WriteFields(AP4_ByteStream& stream)
{
    AP4_Result result;

    // if we don't have a source, write from the buffered payload
    if (m_SourceStream == NULL) {
        return stream.Write(m_Payload.GetData(), m_Payload.GetDataSize());
    }

    // remember the source position
    AP4_Position position;
    m_SourceStream->Tell(position);

    // seek into the source at the stored offset
    result = m_SourceStream->Seek(m_SourcePosition);
    if (AP4_FAILED(result)) return result;

    // copy the source stream to the output
    AP4_UI64 payload_size = GetSize()-GetHeaderSize();
    result = m_SourceStream->CopyTo(stream, payload_size);
    if (AP4_FAILED(result)) return result;

    // restore the original stream position
    m_SourceStream->Seek(position);

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_UnknownAtom::Clone
+---------------------------------------------------------------------*/
AP4_Atom*
AP4_UnknownAtom::Clone()
{
    return new AP4_UnknownAtom(*this);
}

/*----------------------------------------------------------------------
|   AP4_NullTerminatedStringAtom::AP4_NullTerminatedStringAtom
+---------------------------------------------------------------------*/
AP4_NullTerminatedStringAtom::AP4_NullTerminatedStringAtom(AP4_Atom::Type type, const char* value) :
    AP4_Atom(type, AP4_ATOM_HEADER_SIZE),
    m_Value(value)
{
    m_Size32 += m_Value.GetLength()+1;
}

/*----------------------------------------------------------------------
|   AP4_NullTerminatedStringAtom::AP4_NullTerminatedStringAtom
+---------------------------------------------------------------------*/
AP4_NullTerminatedStringAtom::AP4_NullTerminatedStringAtom(AP4_Atom::Type  type,
                                                           AP4_UI64        size,
                                                           AP4_ByteStream& stream) :
    AP4_Atom(type, size)
{
    AP4_Size str_size = (AP4_Size)size - AP4_ATOM_HEADER_SIZE;

    if (str_size) {
        char* str = new char[str_size];
        stream.Read(str, str_size);
        str[str_size - 1] = '\0'; // force null-termination
        m_Value = str;
        delete[] str;
    }
}

/*----------------------------------------------------------------------
|   AP4_NullTerminatedStringAtom::WriteFields
+---------------------------------------------------------------------*/
AP4_Result
AP4_NullTerminatedStringAtom::WriteFields(AP4_ByteStream& stream)
{
    if (m_Size32 > AP4_ATOM_HEADER_SIZE) {
        AP4_Result result = stream.Write(m_Value.GetChars(), m_Value.GetLength()+1);
        if (AP4_FAILED(result)) return result;

        // pad with zeros if necessary
        AP4_Size padding = m_Size32-(AP4_ATOM_HEADER_SIZE+m_Value.GetLength()+1);
        while (padding--) stream.WriteUI08(0);
    }

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_NullTerminatedStringAtom::InspectFields
+---------------------------------------------------------------------*/
AP4_Result
AP4_NullTerminatedStringAtom::InspectFields(AP4_AtomInspector& inspector)
{
    inspector.AddField("string value", m_Value.GetChars());

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::~AP4_AtomParent
+---------------------------------------------------------------------*/
AP4_AtomParent::~AP4_AtomParent()
{
    m_Children.DeleteReferences();
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::AddChild
+---------------------------------------------------------------------*/
AP4_Result
AP4_AtomParent::AddChild(AP4_Atom* child, int position)
{
    // check that the child does not already have a parent
    if (child->GetParent() != NULL) return AP4_ERROR_INVALID_PARAMETERS;

    // attach the child
    AP4_Result result;
    if (position == -1) {
        // insert at the tail
        result = m_Children.Add(child);
    } else if (position == 0) {
        // insert at the head
        result = m_Children.Insert(NULL, child);
    } else {
        // insert after <n-1>
        AP4_List<AP4_Atom>::Item* insertion_point = m_Children.FirstItem();
        unsigned int count = position;
        while (insertion_point && --count) {
            insertion_point = insertion_point->GetNext();
        }
        if (insertion_point) {
            result = m_Children.Insert(insertion_point, child);
        } else {
            result = AP4_ERROR_OUT_OF_RANGE;
        }
    }
    if (AP4_FAILED(result)) return result;

    // notify the child of its parent
    child->SetParent(this);

    // get a chance to update
    OnChildAdded(child);

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::RemoveChild
+---------------------------------------------------------------------*/
AP4_Result
AP4_AtomParent::RemoveChild(AP4_Atom* child)
{
    // check that this is our child
    if (child->GetParent() != this) return AP4_ERROR_INVALID_PARAMETERS;

    // remove the child
    AP4_Result result = m_Children.Remove(child);
    if (AP4_FAILED(result)) return result;

    // notify that child that it is orphaned
    child->SetParent(NULL);

    // get a chance to update
    OnChildRemoved(child);

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::DeleteChild
+---------------------------------------------------------------------*/
AP4_Result
AP4_AtomParent::DeleteChild(AP4_Atom::Type type, AP4_Ordinal index /* = 0 */)
{
    // find the child
    AP4_Atom* child = GetChild(type, index);
    if (child == NULL) return AP4_FAILURE;

    // remove the child
    AP4_Result result = RemoveChild(child);
    if (AP4_FAILED(result)) return result;

    // delete the child
    delete child;

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::GetChild
+---------------------------------------------------------------------*/
AP4_Atom*
AP4_AtomParent::GetChild(AP4_Atom::Type type, AP4_Ordinal index /* = 0 */) const
{
    AP4_Atom* atom;
    AP4_Result result = m_Children.Find(AP4_AtomFinder(type, index), atom);
    if (AP4_SUCCEEDED(result)) {
        return atom;
    } else {
        return NULL;
    }
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::GetChild
+---------------------------------------------------------------------*/
AP4_Atom*
AP4_AtomParent::GetChild(const AP4_UI08* uuid, AP4_Ordinal index /* = 0 */) const
{
    for (AP4_List<AP4_Atom>::Item* item = m_Children.FirstItem();
                                   item;
                                   item = item->GetNext()) {
        AP4_Atom* atom = item->GetData();
        if (atom->GetType() == AP4_ATOM_TYPE_UUID) {
            AP4_UuidAtom* uuid_atom = AP4_DYNAMIC_CAST(AP4_UuidAtom, atom);
            if (AP4_CompareMemory(uuid_atom->GetUuid(), uuid, 16) == 0) {
                if (index == 0) return atom;
                --index;
            }
        }
    }
    return NULL;
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::FindChild
+---------------------------------------------------------------------*/
AP4_Atom*
AP4_AtomParent::FindChild(const char* path,
                          bool        auto_create,
                          bool        auto_create_full)
{
    // start from here
    AP4_AtomParent* parent = this;

    // walk the path
    while (path[0] && path[1] && path[2] && path[3]) {
        // we have 4 valid chars
        const char* end = &path[4];

        // look for the end or a separator
        while (*end != '\0' && *end != '/' && *end != '[') {
            ++end;
        }

        // decide if this is a 4-character code or a UUID
        AP4_UI08 uuid[16];
        AP4_Atom::Type type = 0;
        bool is_uuid = false;
        if (end == path+4) {
            // 4-character code
            type = AP4_ATOM_TYPE(path[0], path[1], path[2], path[3]);
        } else if (end == path+32) {
            // UUID
            is_uuid = true;
            AP4_ParseHex(path, uuid, sizeof(uuid));
        } else {
            // malformed path
            return NULL;
        }

        // parse the array index, if any
        int index = 0;
        if (*end == '[') {
            const char* x = end+1;
            while (*x >= '0' && *x <= '9') {
                index = 10*index+(*x++ - '0');
            }
            if (*x != ']') {
                // malformed path
                return NULL;
            }
            end = x+1;
        }

        // check what's at the end now
        if (*end == '/') {
            ++end;
        } else if (*end != '\0') {
            // malformed path
            return NULL;
        }

        // look for this atom in the current list
        AP4_Atom* atom = NULL;
        if (is_uuid) {
            atom = parent->GetChild(uuid, index);
        } else {
            atom = parent->GetChild(type, index);
        }
        if (atom == NULL) {
            // not found
            if (auto_create && (index == 0)) {
                if (auto_create_full) {
                    atom = new AP4_ContainerAtom(type, (AP4_UI32)0, (AP4_UI32)0);
                } else {
                    atom = new AP4_ContainerAtom(type);
                }
                parent->AddChild(atom);
            } else {
                return NULL;
            }
        }

        if (*end) {
            path = end;
            // if this atom is an atom parent, recurse
            parent = AP4_DYNAMIC_CAST(AP4_ContainerAtom, atom);
            if (parent == NULL) return NULL;
        } else {
            return atom;
        }
    }

    // not found
    return NULL;
}

/*----------------------------------------------------------------------
|   AP4_AtomParent::CopyChildren
+---------------------------------------------------------------------*/
AP4_Result
AP4_AtomParent::CopyChildren(AP4_AtomParent& destination) const
{
    for (AP4_List<AP4_Atom>::Item* child = m_Children.FirstItem(); child; child=child->GetNext()) {
        AP4_Atom* child_clone = child->GetData()->Clone();
        destination.AddChild(child_clone);
    }

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_AtomListWriter::Action
+---------------------------------------------------------------------*/
const unsigned int AP4_ATOM_LIST_WRITER_MAX_PADDING=1024;

AP4_Result
AP4_AtomListWriter::Action(AP4_Atom* atom) const
{
    AP4_Position before;
    m_Stream.Tell(before);

    atom->Write(m_Stream);

    AP4_Position after;
    m_Stream.Tell(after);

    AP4_UI64 bytes_written = after-before;
    AP4_ASSERT(bytes_written <= atom->GetSize());
    if (bytes_written < atom->GetSize()) {
        AP4_Debug("WARNING: atom serialized to fewer bytes than declared size\n");
        AP4_UI64 padding = atom->GetSize()-bytes_written;
        if (padding > AP4_ATOM_LIST_WRITER_MAX_PADDING) {
            AP4_Debug("WARNING: padding would be too large\n");
            return AP4_FAILURE;
        } else {
            for (unsigned int i=0; i<padding; i++) {
                m_Stream.WriteUI08(0);
            }
        }
    }

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_MakePrefixString
+---------------------------------------------------------------------*/
static void
AP4_MakePrefixString(unsigned int indent, char* prefix, AP4_Size size)
{
    if (size == 0) return;
    if (indent >= size-1) indent = size-1;
    for (unsigned int i=0; i<indent; i++) {
        prefix[i] = ' ';
    }
    prefix[indent] = '\0';
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::AP4_PrintInspector
+---------------------------------------------------------------------*/
AP4_PrintInspector::AP4_PrintInspector(
    std::shared_ptr<AP4_ByteStream> stream,
    AP4_Cardinal indent) : m_Stream(std::move(stream))
{
    PushContext(Context::TOP_LEVEL);
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::~AP4_PrintInspector
+---------------------------------------------------------------------*/
AP4_PrintInspector::~AP4_PrintInspector()
{

}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::PushContext
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::PushContext(Context::Type type)
{
    m_Contexts.Append(Context(type));
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::PopContext
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::PopContext()
{
    m_Contexts.RemoveLast();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::PrintPrefix
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::PrintPrefix()
{
    if (LastContext().m_Type == Context::COMPACT_OBJECT) {
        if (LastContext().m_ArrayIndex++) {
            m_Stream->WriteString(", ");
        }
        return;
    }

    if (m_Contexts.ItemCount() >= 1) {
        char prefix[256];
        AP4_MakePrefixString((m_Contexts.ItemCount() - 1) * 2, prefix, sizeof(prefix));
        m_Stream->WriteString(prefix);

        if (LastContext().m_Type == Context::ARRAY) {
            char index[32];
            AP4_FormatString(index, sizeof(index), "(%8d) ", (int)LastContext().m_ArrayIndex);
            m_Stream->WriteString(index);
            ++LastContext().m_ArrayIndex;
        }
    }
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::PrintSuffix
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::PrintSuffix()
{
    if (LastContext().m_Type != Context::COMPACT_OBJECT) {
        m_Stream->WriteString("\n");
    }
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::StartAtom
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::StartAtom(const char* name,
                              AP4_UI08    version,
                              AP4_UI32    flags,
                              AP4_Size    header_size,
                              AP4_UI64    size)
{
    PrintPrefix();
    PushContext(Context::ATOM);

    // write atom name
    char info[128];
    char extra[32] = "";
    if (header_size == 28 || header_size == 12 || header_size == 20) {
        if (version && flags) {
            AP4_FormatString(extra, sizeof(extra),
                             ", version=%d, flags=%x",
                             version,
                             flags);
        } else if (version) {
            AP4_FormatString(extra, sizeof(extra),
                             ", version=%d",
                             version);
        } else if (flags) {
            AP4_FormatString(extra, sizeof(extra),
                             ", flags=%x",
                             flags);
        }
    }
    AP4_FormatString(info, sizeof(info),
                     "size=%d+%lld%s",
                     header_size,
                     size-header_size,
                     extra);

    m_Stream->WriteString("[");
    m_Stream->WriteString(name);
    m_Stream->Write("] ", 2);
    m_Stream->WriteString(info);

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::EndAtom
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::EndAtom()
{
    PopContext();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::StartDescriptor
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::StartDescriptor(const char* name,
                                    AP4_Size    header_size,
                                    AP4_UI64    size)
{
    PrintPrefix();
    PushContext(Context::ATOM);

    // write atom name
    char info[128];
    AP4_FormatString(info, sizeof(info),
                     "size=%d+%lld",
                     header_size,
                     size-header_size);

    m_Stream->Write("[", 1);
    m_Stream->WriteString(name);
    m_Stream->Write("] ", 2);
    m_Stream->WriteString(info);

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::EndDescriptor
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::EndDescriptor()
{
    EndAtom();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::StartArray
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::StartArray(const char* name, unsigned int /* element_count */)
{
    PrintPrefix();
    PushContext(Context::ARRAY);

    if (name) {
        m_Stream->WriteString(name);
        m_Stream->WriteString(":");
    }

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::EndArray
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::EndArray()
{
    PopContext();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::StartObject
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::StartObject(const char* name, unsigned int /* field_count */, bool compact)
{
    PrintPrefix();
    PushContext(compact ? Context::COMPACT_OBJECT : Context::OBJECT);

    if (name) {
        m_Stream->WriteString(name);
        m_Stream->WriteString(": ");
    }

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::EndObject
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::EndObject()
{
    if (LastContext().m_Type == Context::COMPACT_OBJECT) {
        m_Stream->WriteString("\n");
    }
    PopContext();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::AddField
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::AddField(const char* name, const char* value, FormatHint)
{
    PrintPrefix();

    if (name) {
        m_Stream->WriteString(name);
        m_Stream->WriteString(" = ");
    }
    m_Stream->WriteString(value);

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::AddField
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::AddField(const char* name, AP4_UI64 value, FormatHint hint)
{
    PrintPrefix();

    if (name) {
        m_Stream->WriteString(name);
        m_Stream->WriteString(" = ");
    }
    char str[32];
    AP4_FormatString(str, sizeof(str),
                     hint == HINT_HEX ? "%llx":"%lld",
                     value);
    m_Stream->WriteString(str);

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::AddFieldF
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::AddFieldF(const char* name, float value, FormatHint /*hint*/)
{
    PrintPrefix();

    if (name) {
        m_Stream->WriteString(name);
        m_Stream->WriteString(" = ");
    }
    char str[32];
    AP4_FormatString(str, sizeof(str),
                     "%f",
                     value);
    m_Stream->WriteString(str);

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_PrintInspector::AddField
+---------------------------------------------------------------------*/
void
AP4_PrintInspector::AddField(const char*          name,
                             const unsigned char* bytes,
                             AP4_Size             byte_count,
                             FormatHint           /* hint */)
{
    PrintPrefix();

    if (name) {
        m_Stream->WriteString(name);
        m_Stream->WriteString(" = ");
    }
    m_Stream->WriteString("[");
    unsigned int offset = 1;
    char byte[4];
    for (unsigned int i=0; i<byte_count; i++) {
        AP4_FormatString(byte, 4, " %02x", bytes[i]);
        m_Stream->Write(&byte[offset], 3-offset);
        offset = 0;
    }
    m_Stream->WriteString("]");

    PrintSuffix();
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::AP4_JsonInspector
+---------------------------------------------------------------------*/
AP4_JsonInspector::AP4_JsonInspector(std::shared_ptr<AP4_ByteStream> stream) :
    m_Stream(std::move(stream))
{
    m_Stream->WriteString("[\n");
    PushContext(Context::TOP_LEVEL);
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::~AP4_JsonInspector
+---------------------------------------------------------------------*/
AP4_JsonInspector::~AP4_JsonInspector()
{
    m_Stream->WriteString("\n]\n");
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::PushContext
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::PushContext(Context::Type type)
{
    m_Contexts.Append(Context(type));
    AP4_MakePrefixString(m_Contexts.ItemCount() * 2, m_Prefix, sizeof(m_Prefix));
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::PopContext
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::PopContext()
{
    m_Contexts.RemoveLast();
    AP4_MakePrefixString(m_Contexts.ItemCount() * 2, m_Prefix, sizeof(m_Prefix));
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::OnFieldAdded
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::OnFieldAdded()
{
    if (LastContext().m_FieldCount) {
        m_Stream->WriteString(",\n");
    }
    ++LastContext().m_FieldCount;
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::PrintFieldName
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::PrintFieldName(const char* name)
{
    if (!name) return;
    m_Stream->WriteString("\"");
    m_Stream->WriteString(EscapeString(name).GetChars());
    m_Stream->WriteString("\": ");
}

/*----------------------------------------------------------------------
|   Read one code point from a UTF-8 string
+---------------------------------------------------------------------*/
static AP4_Result
ReadUTF8(const AP4_UI08* utf, AP4_Size* length, AP4_UI32* code_point) {
    if (*length < 1) {
        return AP4_ERROR_NOT_ENOUGH_DATA;
    }

    AP4_UI32 c = utf[0];
    if ((c & 0x80) == 0) {
        *length = 1;
        *code_point = c;
        return AP4_SUCCESS;
    }
    if (*length < 2) {
        return AP4_ERROR_NOT_ENOUGH_DATA;
    }
    *code_point = 0;
    if ((utf[1] & 0xc0) != 0x80) {
        return AP4_ERROR_INVALID_FORMAT;
    }
    if ((c & 0xe0) == 0xe0) {
        if (*length < 3) {
            return AP4_ERROR_NOT_ENOUGH_DATA;
        }
        if ((utf[2] & 0xc0) != 0x80) {
            return AP4_ERROR_INVALID_FORMAT;
        }
        if ((c & 0xf0) == 0xf0) {
            if (*length < 4) {
                return AP4_ERROR_NOT_ENOUGH_DATA;
            }
            if ((c & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80) {
                return AP4_ERROR_INVALID_FORMAT;
            }
            *length = 4;
            c  = (utf[0] & 0x07) << 18;
            c |= (utf[1] & 0x3f) << 12;
            c |= (utf[2] & 0x3f) <<  6;
            c |= (utf[3] & 0x3f);
        } else {
            *length = 3;
            c  = (utf[0] & 0x0f) << 12;
            c |= (utf[1] & 0x3f) << 6;
            c |= (utf[2] & 0x3f);
        }
    } else {
      /* 2-byte code */
        *length = 2;
        c  = (utf[0] & 0x1f) << 6;
        c |= (utf[1] & 0x3f);
    }

    *code_point = c;
    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::EscapeString
|
|   Not very efficient but simple function to escape characters in a
|   JSON string
+---------------------------------------------------------------------*/
AP4_String
AP4_JsonInspector::EscapeString(const char* string)
{
    AP4_String result(string);

    // Shortcut
    if (result.GetLength() == 0) {
        return result;
    }

    // Compute the output size
    AP4_Size string_length = (AP4_Size)strlen(string);
    const AP4_UI08* input = (const AP4_UI08*)string;
    AP4_Size input_length = string_length;
    AP4_Size output_size = 0;
    while (input_length) {
        AP4_Size chars_available = input_length;
        AP4_UI32 code_point = 0;
        AP4_Result r = ReadUTF8(input, &chars_available, &code_point);
        if (AP4_FAILED(r)) {
            // stop, but don't fail
            break;
        }
        if (code_point == '"' || code_point == '\\') {
            output_size += 2;
        } else if (code_point <= 0x1F) {
            output_size += 6;
        } else {
            output_size += chars_available;
        }
        input_length -= chars_available;
        input        += chars_available;
    }

    // Shortcut
    if (output_size == result.GetLength()) {
        return result;
    }

    // Compute the escaped string in a temporary buffer
    char* buffer = new char[output_size];
    char* escaped = buffer;
    input = (const AP4_UI08*)string;
    input_length = string_length;
    while (input_length) {
        AP4_Size chars_available = input_length;
        AP4_UI32 code_point = 0;
        AP4_Result r = ReadUTF8(input, &chars_available, &code_point);
        if (AP4_FAILED(r)) {
            // stop, but don't fail
            break;
        }
        if (code_point == '"' || code_point == '\\') {
            *escaped++ = '\\';
            *escaped++ = (char)code_point;
        } else if (code_point <= 0x1F) {
            *escaped++ = '\\';
            *escaped++ = 'u';
            *escaped++ = '0';
            *escaped++ = '0';
            *escaped++ = AP4_NibbleHex(code_point >> 4);
            *escaped++ = AP4_NibbleHex(code_point & 0x0F);
        } else {
            for (AP4_Size i = 0; i < chars_available; i++) {
                *escaped++ = (char)input[i];
            }
        }
        input_length -= chars_available;
        input        += chars_available;
    }

    // Copy the buffer to a final string
    result.Assign(buffer, output_size);
    delete[] buffer;

    return result;
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::StartAtom
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::StartAtom(const char* name,
                             AP4_UI08    version,
                             AP4_UI32    flags,
                             AP4_Size    header_size,
                             AP4_UI64    size)
{
    OnFieldAdded();
    ++LastContext().m_ChildrenCount;

    // Starting the first atom within an atom means staring a childen array
    if (LastContext().m_Type == Context::ATOM && LastContext().m_ChildrenCount == 1) {
        m_Stream->WriteString(m_Prefix);
        m_Stream->WriteString("\"children\":[ \n");
    }

    m_Stream->WriteString(m_Prefix);
    m_Stream->WriteString("{\n");
    PushContext(Context::ATOM);

    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    PrintFieldName("name");
    m_Stream->WriteString("\"");
    m_Stream->WriteString(EscapeString(name).GetChars());
    m_Stream->WriteString("\"");

    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    PrintFieldName("header_size");
    char val[32];
    AP4_FormatString(val, sizeof(val), "%d", header_size);
    m_Stream->WriteString(val);

    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    PrintFieldName("size");
    AP4_FormatString(val, sizeof(val), "%lld", size);
    m_Stream->WriteString(val);

    if (version) {
        OnFieldAdded();
        m_Stream->WriteString(m_Prefix);
        PrintFieldName("version");
        AP4_FormatString(val, sizeof(val), "%d", version);
        m_Stream->WriteString(val);
    }

    if (flags) {
        OnFieldAdded();
        m_Stream->WriteString(m_Prefix);
        PrintFieldName("flags");
        AP4_FormatString(val, sizeof(val), "%d", flags);
        m_Stream->WriteString(val);
    }
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::EndAtom
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::EndAtom()
{
    // Ending an atom with children means we need to close the children array
    if (LastContext().m_ChildrenCount) {
        m_Stream->WriteString("]");
    }

    PopContext();

    m_Stream->WriteString("\n");
    m_Stream->WriteString(m_Prefix);
    m_Stream->WriteString("}");
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::StartDescriptor
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::StartDescriptor(const char* name,
                                   AP4_Size    header_size,
                                   AP4_UI64    size)
{
    StartAtom(name, 0, 0, header_size, size);
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::EndDescriptor
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::EndDescriptor()
{
    EndAtom();
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::StartArray
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::StartArray(const char* name, unsigned int /* element_count */)
{
    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    if (name) {
        PrintFieldName(name);
    }

    m_Stream->WriteString("[\n");
    PushContext(Context::ARRAY);
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::EndArray
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::EndArray()
{
    PopContext();
    m_Stream->WriteString("\n");
    m_Stream->WriteString(m_Prefix);
    m_Stream->WriteString("]");
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::StartObject
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::StartObject(const char* name, unsigned int /* field_count */, bool /* compact */)
{
    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    if (name) {
        PrintFieldName(name);
    }

    m_Stream->WriteString("{\n");
    PushContext(Context::ARRAY);
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::EndObject
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::EndObject()
{
    PopContext();
    m_Stream->WriteString("\n");
    m_Stream->WriteString(m_Prefix);
    m_Stream->WriteString("}");
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::AddField
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::AddField(const char* name, const char* value, FormatHint)
{
    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    PrintFieldName(name);
    m_Stream->WriteString("\"");
    m_Stream->WriteString(EscapeString(value).GetChars());
    m_Stream->WriteString("\"");
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::AddField
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::AddField(const char* name, AP4_UI64 value, FormatHint /* hint */)
{
    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    PrintFieldName(name);
    char str[32];
    AP4_FormatString(str, sizeof(str),
                     "%lld",
                     value);
    m_Stream->WriteString(str);
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::AddFieldF
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::AddFieldF(const char* name, float value, FormatHint /*hint*/)
{
    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    PrintFieldName(name);
    char str[32];
    AP4_FormatString(str, sizeof(str),
                     "%f",
                     value);
    m_Stream->WriteString(str);
}

/*----------------------------------------------------------------------
|   AP4_JsonInspector::AddField
+---------------------------------------------------------------------*/
void
AP4_JsonInspector::AddField(const char*          name,
                            const unsigned char* bytes,
                            AP4_Size             byte_count,
                            FormatHint           /* hint */)
{
    OnFieldAdded();
    m_Stream->WriteString(m_Prefix);
    PrintFieldName(name);
    m_Stream->WriteString("\"[");
    unsigned int offset = 1;
    char byte[4];
    for (unsigned int i = 0; i < byte_count; i++) {
        AP4_FormatString(byte, 4, " %02x", bytes[i]);
        m_Stream->Write(&byte[offset], 3 - offset);
        offset = 0;
    }
    m_Stream->WriteString("]\"");
}


