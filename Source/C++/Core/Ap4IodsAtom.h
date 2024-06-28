/*****************************************************************
|
|    AP4 - iods Atom
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

#ifndef _AP4_IODS_ATOM_H_
#define _AP4_IODS_ATOM_H_

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "Ap4Types.h"
#include "Ap4Atom.h"
#include "Ap4ObjectDescriptor.h"

#include <memory>

/*----------------------------------------------------------------------
|   class references
+---------------------------------------------------------------------*/
class AP4_ByteStream;

/*----------------------------------------------------------------------
|   AP4_IodsAtom
+---------------------------------------------------------------------*/
class AP4_IodsAtom : public AP4_Atom
{
public:
    AP4_IMPLEMENT_DYNAMIC_CAST_D(AP4_IodsAtom, AP4_Atom)

    // class methods
    static AP4_IodsAtom* Create(AP4_Size size, std::shared_ptr<AP4_ByteStream> stream);

    // methods
    AP4_IodsAtom(AP4_ObjectDescriptor* descriptor); // ownership of 'descriptor' is transfered
   ~AP4_IodsAtom();
    virtual AP4_Result WriteFields(AP4_ByteStream& stream);
    virtual AP4_Result InspectFields(AP4_AtomInspector& inspector);
    const AP4_ObjectDescriptor* GetObjectDescriptor() const { return m_ObjectDescriptor; }
    
private:
    // methods
    AP4_IodsAtom(AP4_UI32        size, 
                 AP4_UI08        version,
                 AP4_UI32        flags,
                 std::shared_ptr<AP4_ByteStream> stream);

    // members
    AP4_ObjectDescriptor* m_ObjectDescriptor;
};

#endif // _AP4_IODS_ATOM_H_
