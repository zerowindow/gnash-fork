// Proxy_as.cpp:  ActionScript "Proxy" class, for Gnash.
//
//   Copyright (C) 2009, 2010 Free Software Foundation, Inc.
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


#include "utils/Proxy_as.h"
#include "log.h"
#include "fn_call.h"
#include "Global_as.h"
#include "smart_ptr.h" // for boost intrusive_ptr
#include "builtin_function.h" // need builtin_function
#include "GnashException.h" // for ActionException

namespace gnash {

// Forward declarations
namespace {
    as_value proxy_deleteProperty(const fn_call& fn);
    as_value proxy_getDescendants(const fn_call& fn);
    as_value proxy_getProperty(const fn_call& fn);
    as_value proxy_hasProperty(const fn_call& fn);
    as_value proxy_isAttribute(const fn_call& fn);
    as_value proxy_nextName(const fn_call& fn);
    as_value proxy_nextNameIndex(const fn_call& fn);
    as_value proxy_nextValue(const fn_call& fn);
    as_value proxy_setProperty(const fn_call& fn);
    as_value proxy_ctor(const fn_call& fn);
    void attachProxyInterface(as_object& o);
    void attachProxyStaticInterface(as_object& o);
}

// extern (used by Global.cpp)
void
proxy_class_init(as_object& where, const ObjectURI& uri)
{
    registerBuiltinClass(where, proxy_ctor, attachProxyInterface,
            attachProxyStaticInterface, uri);
}

namespace {

void
attachProxyInterface(as_object& o)
{
    Global_as& gl = getGlobal(o);
    o.init_member("deleteProperty", gl.createFunction(proxy_deleteProperty));
    o.init_member("getDescendants", gl.createFunction(proxy_getDescendants));
    o.init_member("getProperty", gl.createFunction(proxy_getProperty));
    o.init_member("hasProperty", gl.createFunction(proxy_hasProperty));
    o.init_member("isAttribute", gl.createFunction(proxy_isAttribute));
    o.init_member("nextName", gl.createFunction(proxy_nextName));
    o.init_member("nextNameIndex", gl.createFunction(proxy_nextNameIndex));
    o.init_member("nextValue", gl.createFunction(proxy_nextValue));
    o.init_member("setProperty", gl.createFunction(proxy_setProperty));
}

void
attachProxyStaticInterface(as_object& /*o*/)
{

}

as_value
proxy_deleteProperty(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_getDescendants(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_getProperty(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_hasProperty(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_isAttribute(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_nextName(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_nextNameIndex(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_nextValue(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_setProperty(const fn_call& /*fn*/)
{
    log_unimpl (__FUNCTION__);
    return as_value();
}

as_value
proxy_ctor(const fn_call& /*fn*/)
{
    return as_value(); 
}

} // anonymous namespace 
} // gnash namespace

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:

