/*****************************************************************
|
|    AP4 - ES Descriptors
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
#include "Ap4EsDescriptor.h"
#include "Ap4DescriptorFactory.h"
#include "Ap4Utils.h"
#include "Ap4ByteStream.h"
#include "Ap4Atom.h"

/*----------------------------------------------------------------------
|   dynamic cast support
+---------------------------------------------------------------------*/
AP4_DEFINE_DYNAMIC_CAST_ANCHOR(AP4_EsDescriptor)
AP4_DEFINE_DYNAMIC_CAST_ANCHOR(AP4_EsIdIncDescriptor)
AP4_DEFINE_DYNAMIC_CAST_ANCHOR(AP4_EsIdRefDescriptor)

/*----------------------------------------------------------------------
|   AP4_EsDescriptor::AP4_EsDescriptor
+---------------------------------------------------------------------*/
AP4_EsDescriptor::AP4_EsDescriptor(AP4_UI16 es_id) :
    AP4_Descriptor(AP4_DESCRIPTOR_TAG_ES, 2, 2+1),
    m_EsId(es_id),
    m_OcrEsId(0),
    m_Flags(0),
    m_StreamPriority(0),
    m_DependsOn(0)
{
}

/*----------------------------------------------------------------------
|   AP4_EsDescriptor::AP4_EsDescriptor
+---------------------------------------------------------------------*/
AP4_EsDescriptor::AP4_EsDescriptor(std::shared_ptr<AP4_ByteStream> stream,
                                   AP4_Size                        header_size,
                                   AP4_Size                        payload_size) :
    AP4_Descriptor(AP4_DESCRIPTOR_TAG_ES, header_size, payload_size)
{
    // read descriptor fields
    if (payload_size < 3) return;
    stream->ReadUI16(m_EsId);
    unsigned char bits;
    stream->ReadUI08(bits);
    payload_size -= 3;
    m_Flags = (bits>>5)&7;
    m_StreamPriority = bits&0x1F;
    if (m_Flags & AP4_ES_DESCRIPTOR_FLAG_STREAM_DEPENDENCY) {
        if (payload_size < 2) return;
        stream->ReadUI16(m_DependsOn);
        payload_size -= 2;
    }  else {
        m_DependsOn = 0;
    }
    if (m_Flags & AP4_ES_DESCRIPTOR_FLAG_URL) {
        unsigned char url_length;
        if (payload_size < 1) return;
        stream->ReadUI08(url_length);
        --payload_size;
        if (url_length) {
            if (payload_size < url_length) return;
            char* url = new char[url_length+1];
            if (url) {
                stream->Read(url, url_length);
                url[url_length] = '\0';
                m_Url = url;
                delete[] url;
            }
            payload_size -= url_length;
        }
    }
    if (m_Flags & AP4_ES_DESCRIPTOR_FLAG_URL) {
        if (payload_size < 2) return;
        stream->ReadUI16(m_OcrEsId);
        payload_size -= 2;
    } else {
        m_OcrEsId = 0;
    }

    // read other descriptors
    AP4_Position offset;
    stream->Tell(offset);
    auto substream = std::make_shared<AP4_SubStream>(stream, offset, payload_size);
    AP4_Descriptor* descriptor = NULL;
    while (AP4_DescriptorFactory::CreateDescriptorFromStream(substream, 
                                                             descriptor) 
           == AP4_SUCCESS) {
        m_SubDescriptors.Add(descriptor);
    }
}

/*----------------------------------------------------------------------
|   AP4_EsDescriptor::~AP4_EsDescriptor
+---------------------------------------------------------------------*/
AP4_EsDescriptor::~AP4_EsDescriptor()
{
    m_SubDescriptors.DeleteReferences();
}

/*----------------------------------------------------------------------
|   AP4_EsDescriptor::WriteFields
+---------------------------------------------------------------------*/
AP4_Result
AP4_EsDescriptor::WriteFields(AP4_ByteStream& stream)
{
    AP4_Result result;

    // es id
    result = stream.WriteUI16(m_EsId);
    if (AP4_FAILED(result)) return result;

    // flags and other bits
    AP4_UI08 bits = m_StreamPriority | (AP4_UI08)(m_Flags<<5);
    result = stream.WriteUI08(bits);
    if (AP4_FAILED(result)) return result;
    
    // optional fields
    if (m_Flags & AP4_ES_DESCRIPTOR_FLAG_STREAM_DEPENDENCY) {
        result = stream.WriteUI16(m_DependsOn);
        if (AP4_FAILED(result)) return result;
    }
    if (m_Flags & AP4_ES_DESCRIPTOR_FLAG_URL) {
        result = stream.WriteUI08((AP4_UI08)m_Url.GetLength());
        if (AP4_FAILED(result)) return result;
        result = stream.WriteString(m_Url.GetChars());
        if (AP4_FAILED(result)) return result;
        result = stream.WriteUI08(0);
        if (AP4_FAILED(result)) return result;
    }
    if (m_Flags & AP4_ES_DESCRIPTOR_FLAG_OCR_STREAM) {
        result = stream.WriteUI16(m_OcrEsId);
        if (AP4_FAILED(result)) return result;
    }

    // write the sub descriptors
    m_SubDescriptors.Apply(AP4_DescriptorListWriter(stream));

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_EsDescriptor::Inspect
+---------------------------------------------------------------------*/
AP4_Result
AP4_EsDescriptor::Inspect(AP4_AtomInspector& inspector)
{
    inspector.StartDescriptor("ESDescriptor", GetHeaderSize(), GetSize());
    inspector.AddField("es_id", m_EsId);
    inspector.AddField("stream_priority", m_StreamPriority);

    // inspect children
    m_SubDescriptors.Apply(AP4_DescriptorListInspector(inspector));

    inspector.EndDescriptor();

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_EsDescriptor::AddSubDescriptor
+---------------------------------------------------------------------*/
AP4_Result
AP4_EsDescriptor::AddSubDescriptor(AP4_Descriptor* descriptor)
{
    m_SubDescriptors.Add(descriptor);
    m_PayloadSize += descriptor->GetSize();

    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_EsDescriptor::GetDecoderConfigDescriptor
+---------------------------------------------------------------------*/
const AP4_DecoderConfigDescriptor*
AP4_EsDescriptor::GetDecoderConfigDescriptor() const
{
    // find the decoder config descriptor
    AP4_Descriptor* descriptor = NULL;
    AP4_Result result = 
        m_SubDescriptors.Find(AP4_DescriptorFinder(AP4_DESCRIPTOR_TAG_DECODER_CONFIG),
                              descriptor);
    
    // return it
    if (AP4_SUCCEEDED(result)) {
        return AP4_DYNAMIC_CAST(AP4_DecoderConfigDescriptor, descriptor);
    } else {
        return NULL;
    }
}

/*----------------------------------------------------------------------
|   AP4_EsIdIncDescriptor::AP4_EsIdIncDescriptor
+---------------------------------------------------------------------*/
AP4_EsIdIncDescriptor::AP4_EsIdIncDescriptor(AP4_UI32 track_id) :
    AP4_Descriptor(AP4_DESCRIPTOR_TAG_ES_ID_INC, 2, 4),
    m_TrackId(track_id)
{
}

/*----------------------------------------------------------------------
|   AP4_EsIdIncDescriptor::AP4_EsIdIncDescriptor
+---------------------------------------------------------------------*/
AP4_EsIdIncDescriptor::AP4_EsIdIncDescriptor(AP4_ByteStream& stream, 
                                             AP4_Size        header_size,
                                             AP4_Size        payload_size) :
    AP4_Descriptor(AP4_DESCRIPTOR_TAG_ES_ID_INC, header_size, payload_size),
    m_TrackId(0)
{
    // read the track id
    stream.ReadUI32(m_TrackId);
}

/*----------------------------------------------------------------------
|   AP4_EsIdIncescriptor::WriteFields
+---------------------------------------------------------------------*/
AP4_Result
AP4_EsIdIncDescriptor::WriteFields(AP4_ByteStream& stream)
{
    // track id
    return stream.WriteUI32(m_TrackId);
}

/*----------------------------------------------------------------------
|   AP4_EsIdIncDescriptor::Inspect
+---------------------------------------------------------------------*/
AP4_Result
AP4_EsIdIncDescriptor::Inspect(AP4_AtomInspector& inspector)
{
    inspector.StartDescriptor("ES_ID_Inc", GetHeaderSize(), GetSize());
    inspector.AddField("track_id", m_TrackId);
    inspector.EndDescriptor();
    
    return AP4_SUCCESS;
}

/*----------------------------------------------------------------------
|   AP4_EsIdRefDescriptor::AP4_EsIdRefDescriptor
+---------------------------------------------------------------------*/
AP4_EsIdRefDescriptor::AP4_EsIdRefDescriptor(AP4_UI16 ref_index) :
    AP4_Descriptor(AP4_DESCRIPTOR_TAG_ES_ID_REF, 2, 2),
    m_RefIndex(ref_index)
{
}

/*----------------------------------------------------------------------
|   AP4_EsIdRefDescriptor::AP4_EsIdRefDescriptor
+---------------------------------------------------------------------*/
AP4_EsIdRefDescriptor::AP4_EsIdRefDescriptor(AP4_ByteStream& stream, 
                                             AP4_Size        header_size,
                                             AP4_Size        payload_size) :
    AP4_Descriptor(AP4_DESCRIPTOR_TAG_ES_ID_REF, header_size, payload_size),
    m_RefIndex(0)
{
    // read the ref index
    stream.ReadUI16(m_RefIndex);
}

/*----------------------------------------------------------------------
|   AP4_EsIdRefDescriptor::WriteFields
+---------------------------------------------------------------------*/
AP4_Result
AP4_EsIdRefDescriptor::WriteFields(AP4_ByteStream& stream)
{
    // ref index
    return stream.WriteUI16(m_RefIndex);
}

/*----------------------------------------------------------------------
|   AP4_EsIdRefDescriptor::Inspect
+---------------------------------------------------------------------*/
AP4_Result
AP4_EsIdRefDescriptor::Inspect(AP4_AtomInspector& inspector)
{
    inspector.StartDescriptor("ES_ID_Ref", GetHeaderSize(), GetSize());
    inspector.AddField("ref_index", m_RefIndex);
    inspector.EndDescriptor();
    
    return AP4_SUCCESS;
}
