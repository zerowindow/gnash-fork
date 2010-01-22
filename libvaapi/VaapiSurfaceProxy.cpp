// VaapiSurfaceProxy.cpp: VA surface proxy
// 
// Copyright (C) 2007, 2008, 2009, 2010 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

#include "VaapiSurfaceProxy.h"
#include "VaapiSurface.h"
#include "VaapiImage.h"
#include "VaapiContext.h"

#define DEBUG 0
#include "vaapi_debug.h"

namespace gnash {

VaapiSurfaceProxy::VaapiSurfaceProxy(boost::shared_ptr<VaapiSurface> surface,
				     boost::shared_ptr<VaapiContext> context)
    : _context(context), _surface(surface)
{
    D(bug("VaapiSurfaceProxy::VaapiSurfaceProxy(): surface 0x%08x\n", _surface->get()));
}

VaapiSurfaceProxy::~VaapiSurfaceProxy()
{
    D(bug("VaapiSurfaceProxy::~VaapiSurfaceProxy(): surface 0x%08x\n", _surface->get()));
    _context->releaseSurface(_surface);
}

// Return VA image
std::auto_ptr<VaapiImage> VaapiSurfaceProxy::getImage() const
{
    std::auto_ptr<VaapiImage> image(new VaapiImage(_surface.get()));
    return image;
}

} // gnash namespace
