// 
//   Copyright (C) 2005, 2006 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License

// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

// Test case for Boolean ActionScript class
// compile this test case with Ming makeswf, and then
// execute it like this gnash -1 -r 0 -v out.swf

rcsid="$Id: Boolean.as,v 1.10 2006/11/25 14:50:23 strk Exp $";

#include "check.as"

var boolObj = new Boolean;

// test the Boolean constuctor
check (boolObj);

check (boolObj.toString);
check (boolObj.valueOf);

#if OUTPUT_VERSION > 6
// SWF7 and up is case-sensitive
check_equals (boolObj.tostring, undefined);
check_equals (boolObj.valueof, undefined);
#else
check (boolObj.tostring)
check (boolObj.valueof)
#endif

var defaultBool = new Boolean();
check_equals(defaultBool.toString(), "false");
check_equals(defaultBool.valueOf(), false);

var trueBool = new Boolean(true);
check_equals(trueBool.toString(), "true");
check_equals(trueBool.valueOf(), true);

var falseBool = new Boolean(false);
check_equals(falseBool.toString(), "false");
check_equals(falseBool.valueOf(), false);

