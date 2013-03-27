// environment-ctrl.hxx -- controller for environment information.
//
// Written by David Megginson, started May 2002.
//
// Copyright (C) 2002  David Megginson - david@megginson.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//

#ifndef _ENVIRONMENT_CTRL_HXX
#define _ENVIRONMENT_CTRL_HXX

#include <simgear/structure/subsystem_mgr.hxx>

namespace Environment {
    class LayerInterpolateController : public SGSubsystem {
    public:
        static LayerInterpolateController * createInstance( SGPropertyNode_ptr rootNode );
    };
} // namespace

#endif // _ENVIRONMENT_CTRL_HXX
