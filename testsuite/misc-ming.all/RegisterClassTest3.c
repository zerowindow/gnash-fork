#include <stdlib.h>
#include <stdio.h>
#include <ming.h>

#include "ming_utils.h"

#define OUTPUT_VERSION 8
#define OUTPUT_FILENAME "RegisterClassTest3.swf"


int
main(int argc, char** argv)
{
    SWFMovie mo;
    SWFMovieClip mc3, mc2, dejagnuclip;
    SWFAction ac, ac1;
    SWFDisplayItem it;
    SWFShape sha;

    const char *srcdir=".";
    if (argc > 1) srcdir = argv[1];
    else {
        fprintf(stderr, "Usage: %s <mediadir>\n", argv[0]);
        return 1;
    }

    Ming_init();
    mo = newSWFMovieWithVersion(OUTPUT_VERSION);
    SWFMovie_setDimension(mo, 800, 600);
    SWFMovie_setRate (mo, 12.0);

    sha = newSWFShape();

    // Character ID 2. Has 1 showframe. Is exported first.
    mc2 = newSWFMovieClip();
    SWFMovieClip_add(mc2, (SWFBlock)sha);
    SWFMovieClip_nextFrame(mc2);

    // Export it.
    SWFMovie_addExport(mo, (SWFBlock)mc2, "C2");
    SWFMovie_writeExports(mo);

    // Main timeline actions for frame 1
    add_actions(mo, "var c = 0; trace('frame 1'); gotoAndStop(3);");
    
    // ID 3 is defined here. It has no showframe. It is exported immediately.
    mc3 = newSWFMovieClip();
    SWFMovie_addExport(mo, (SWFBlock)mc3, "ctor");
    SWFMovie_writeExports(mo);

    dejagnuclip = get_dejagnu_clip((SWFBlock)get_default_font(srcdir), 10,
    		    0, 0, 800, 600);
    SWFMovie_add(mo, (SWFBlock)dejagnuclip);

    // Init actions for ID 3
    ac = newSWFAction(
    "   _global.ctor = function () {"
    "       super();"
    "       trace('Object in Frame 2 is constructed');"
    "       c += 1;"
    "   };"
    );
    SWFInitAction ia = newSWFInitAction_withId(ac, 3);
    SWFMovie_add(mo, (SWFBlock)ia);
    
    // Init actions for ID 2 (registered class)
    ac1 = newSWFAction("Object.registerClass('C2', ctor); "
            "trace('Registered class');");
    ia = newSWFInitAction_withId(ac1, 2);
    SWFMovie_add(mo, (SWFBlock)ia);
	
    
    // Frame 2
    SWFMovie_nextFrame(mo);
    add_actions(mo, "trace('Frame 2');");
    
    // Place object ID 2.
    it = SWFMovie_add(mo, (SWFBlock)mc2);
    SWFDisplayItem_setName(it, "mc2");

    // Frame 3
    SWFMovie_nextFrame(mo);

    // Remove object ID 2
    SWFMovie_remove(mo, it);

    add_actions(mo, "trace('frame 3');");
    
    // The class should not be constructed if the object is removed after
    // being placed. It should be constructed if it's not removed.
    xcheck(mo, "c == 0");
    check(mo, "_root.mc2 == undefined");
    add_actions(mo, "gotoAndStop(5);");

    // Frame 4
    SWFMovie_nextFrame(mo);
    add_actions(mo, "trace('Frame 4');");
    
    // Place object ID 2 again
    it = SWFMovie_add(mo, (SWFBlock)mc2);
    SWFDisplayItem_setName(it, "mc2a");

    // Frame 5
    SWFMovie_nextFrame(mo);

    // This time the MovieClip was not removed before we get here,
    // so it should be present and the constructor should be
    // called.
    add_actions(mo, "trace('frame 5');");
    xcheck(mo, "c == 1");
    check(mo, "typeof(_root.mc2a) == 'movieclip'");

    SWFMovie_nextFrame(mo);
  
    puts("Saving " OUTPUT_FILENAME );
    SWFMovie_save(mo, OUTPUT_FILENAME);

    return 0;
}

