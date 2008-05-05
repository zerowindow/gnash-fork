// FileReference_as.cpp:  ActionScript "FileReference" class, for Gnash.
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

#ifdef HAVE_CONFIG_H
#include "gnashconfig.h"
#endif

#include "FileReference_as.h"
#include "as_object.h" // for inheritance
#include "log.h"
#include "fn_call.h"
#include "smart_ptr.h" // for boost intrusive_ptr
#include "builtin_function.h" // need builtin_function
#include "GnashException.h" // for ActionException
#include "Object.h" // for AS inheritance

#include <sstream>

namespace gnash {

static as_value FileReference_addListener(const fn_call& fn);
static as_value FileReference_browse(const fn_call& fn);
static as_value FileReference_cancel(const fn_call& fn);
static as_value FileReference_download(const fn_call& fn);
static as_value FileReference_removeListener(const fn_call& fn);
static as_value FileReference_upload(const fn_call& fn);
as_value FileReference_ctor(const fn_call& fn);

static void
attachFileReferenceInterface(as_object& o)
{
    o.init_member("addListener", new builtin_function(FileReference_addListener));
    o.init_member("browse", new builtin_function(FileReference_browse));
    o.init_member("cancel", new builtin_function(FileReference_cancel));
    o.init_member("download", new builtin_function(FileReference_download));
    o.init_member("removeListener", new builtin_function(FileReference_removeListener));
    o.init_member("upload", new builtin_function(FileReference_upload));
}

static as_object*
getFileReferenceInterface()
{
	static boost::intrusive_ptr<as_object> o;
	if ( ! o )
	{
		// TODO: check if this class should inherit from Object
		//       or from a different class
		o = new as_object(getObjectInterface());
		attachFileReferenceInterface(*o);
	}
	return o.get();
}

class FileReference_as: public as_object
{

public:

	FileReference_as()
		:
		as_object(getFileReferenceInterface())
	{}

	// override from as_object ?
	//std::string get_text_value() const { return "FileReference"; }

	// override from as_object ?
	//double get_numeric_value() const { return 0; }
};


static as_value
FileReference_addListener(const fn_call& fn)
{
	boost::intrusive_ptr<FileReference_as> ptr = ensureType<FileReference_as>(fn.this_ptr);
	UNUSED(ptr);
	LOG_ONCE( log_unimpl (__FUNCTION__) );
	return as_value();
}

static as_value
FileReference_browse(const fn_call& fn)
{
	boost::intrusive_ptr<FileReference_as> ptr = ensureType<FileReference_as>(fn.this_ptr);
	UNUSED(ptr);
	LOG_ONCE( log_unimpl (__FUNCTION__) );
	return as_value();
}

static as_value
FileReference_cancel(const fn_call& fn)
{
	boost::intrusive_ptr<FileReference_as> ptr = ensureType<FileReference_as>(fn.this_ptr);
	UNUSED(ptr);
	LOG_ONCE( log_unimpl (__FUNCTION__) );
	return as_value();
}

static as_value
FileReference_download(const fn_call& fn)
{
	boost::intrusive_ptr<FileReference_as> ptr = ensureType<FileReference_as>(fn.this_ptr);
	UNUSED(ptr);
	LOG_ONCE( log_unimpl (__FUNCTION__) );
	return as_value();
}

static as_value
FileReference_removeListener(const fn_call& fn)
{
	boost::intrusive_ptr<FileReference_as> ptr = ensureType<FileReference_as>(fn.this_ptr);
	UNUSED(ptr);
	LOG_ONCE( log_unimpl (__FUNCTION__) );
	return as_value();
}

static as_value
FileReference_upload(const fn_call& fn)
{
	boost::intrusive_ptr<FileReference_as> ptr = ensureType<FileReference_as>(fn.this_ptr);
	UNUSED(ptr);
	LOG_ONCE( log_unimpl (__FUNCTION__) );
	return as_value();
}

as_value
FileReference_ctor(const fn_call& fn)
{
	boost::intrusive_ptr<as_object> obj = new FileReference_as;

	if ( fn.nargs )
	{
		std::stringstream ss;
		fn.dump_args(ss);
		LOG_ONCE( log_unimpl("FileReference(%s): %s", ss.str(), _("arguments discarded")) );
	}

	return as_value(obj.get()); // will keep alive
}

// extern (used by Global.cpp)
void FileReference_class_init(as_object& global)
{
	// This is going to be the global FileReference "class"/"function"
	static boost::intrusive_ptr<builtin_function> cl;

	if ( cl == NULL )
	{
		cl=new builtin_function(&FileReference_ctor, getFileReferenceInterface());
		// replicate all interface to class, to be able to access
		// all methods as static functions
		attachFileReferenceInterface(*cl);
	}

	// Register _global.FileReference
	global.init_member("FileReference", cl.get());
}

} // end of gnash namespace