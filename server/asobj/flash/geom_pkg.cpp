// geom_pkg.cpp:  ActionScript "flash.geom" package, for Gnash.
// 
//   Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
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

#include "Object.h" // for getObjectInterface
#include "as_object.h"

#include "flash/geom/ColorTransform_as.h"
#include "flash/geom/Matrix_as.h"
#include "flash/geom/Point_as.h"
#include "flash/geom/Rectangle_as.h"
#include "flash/geom/Transform_as.h"

namespace gnash {

void
flash_geom_package_init(as_object& where)
{
	as_object* pkg = new as_object(getObjectInterface());

	ColorTransform_class_init(*pkg);
	Matrix_class_init(*pkg);
	Point_class_init(*pkg);
	Rectangle_class_init(*pkg);
	Transform_class_init(*pkg);

	where.init_member("geom", pkg);
}


} // end of gnash namespace