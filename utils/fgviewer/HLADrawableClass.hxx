// Copyright (C) 2009 - 2012  Mathias Froehlich - Mathias.Froehlich@web.de
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//

#ifndef HLADrawableClass_hxx
#define HLADrawableClass_hxx

#include <string>
#include <simgear/hla/HLAObjectClass.hxx>

namespace fgviewer {

class HLADrawableClass : public simgear::HLAObjectClass {
public:
    HLADrawableClass(const std::string& name, simgear::HLAFederate* federate);
    virtual ~HLADrawableClass();

    /// Create a new instance of this class.
    virtual simgear::HLAObjectInstance* createObjectInstance(const std::string& name);

    virtual void createAttributeDataElements(simgear::HLAObjectInstance& objectInstance);

    bool setNameIndex(const std::string& path)
    { return getDataElementIndex(_nameIndex, path); }
    const simgear::HLADataElementIndex& getNameIndex() const
    { return _nameIndex; }

    bool setRendererIndex(const std::string& path)
    { return getDataElementIndex(_rendererIndex, path); }
    const simgear::HLADataElementIndex& getRendererIndex() const
    { return _rendererIndex; }

    bool setDisplayIndex(const std::string& path)
    { return getDataElementIndex(_displayIndex, path); }
    const simgear::HLADataElementIndex& getDisplayIndex() const
    { return _displayIndex; }

private:
    simgear::HLADataElementIndex _nameIndex;
    simgear::HLADataElementIndex _rendererIndex;
    simgear::HLADataElementIndex _displayIndex;
};

} // namespace fgviewer

#endif
