// 
//   Copyright (C) 2005, 2006 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

// Linking Gnash statically or dynamically with other modules is making a
// combined work based on Gnash. Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Gnash give you
// permission to combine Gnash with free software programs or libraries
// that are released under the GNU LGPL and with code included in any
// release of Talkback distributed by the Mozilla Foundation. You may
// copy and distribute such a system following the terms of the GNU GPL
// for all but the LGPL-covered parts and Talkback, and following the
// LGPL for the LGPL-covered parts.
//
// Note that people who make modified versions of Gnash are not obligated
// to grant this special exception for their modified versions; it is their
// choice whether to do so. The GNU General Public License gives permission
// to release a modified version without this exception; this exception
// also makes it possible to release a modified version which carries
// forward this exception.
// 
//

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Xlib/Xt stuff */
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#ifdef HAVE_GTK2
#include <gtk/gtk.h>
#endif
#include <string>
#include <map>
#include "pluginbase.h"
#include "prlock.h"
#include "prcvar.h"
#include "prthread.h"

/* ascii codes for various special keys */
#define ESCAPE 27
#define PAGE_UP 73
#define PAGE_DOWN 81
#define UP_ARROW 72
#define DOWN_ARROW 80
#define LEFT_ARROW 75
#define RIGHT_ARROW 77

extern NPBool      plugInitialized;
extern PRLock      *playerMutex;
extern PRCondVar   *playerCond;

class nsPluginInstance : public nsPluginInstanceBase
{
public:
    nsPluginInstance(NPP aInstance);
    virtual ~nsPluginInstance();

    // We are required to implement these three methods.
    NPBool init(NPWindow *aWindow);
    NPBool isInitialized() {return plugInitialized;}
    void shut();

    NPError GetValue(NPPVariable variable, void *value);
    NPError SetWindow(NPWindow *aWindow);

    NPError NewStream(NPMIMEType type, NPStream *stream, NPBool seekable,
                      uint16 *stype);
    NPError DestroyStream(NPStream * stream, NPError reason);

    int32 WriteReady(NPStream *stream);
    int32 Write(NPStream *stream, int32 offset, int32 len, void *buffer);

    NPError WriteStatus(char *msg) const;
    NPError WriteStatus(std::string msg) const;

    int startProc(std::string filespec, Window win);

private:
    NPP                                _instance;
    Window                             _window;
    std::string                        _swf_file;
    unsigned int                       _width;
    unsigned int                       _height;
    std::map<std::string, std::string> _options;
    int                                _streamfd;
    pid_t                              _childpid;
};

// end of __PLUGIN_H__
#endif

// Local Variables:
// mode: C++
// End:
