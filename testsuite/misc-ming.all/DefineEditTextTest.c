/* 
 *   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 */ 

/*
 * Test DefineEditText tag.
 * Uses both "embedded" font and device fonts.
 * The text written is 'Hello world' in both cases.
 * Text at the bottom is the one with embedded fonts.
 *
 * run as ./DefineEditTextTest
 */

#include <stdlib.h>
#include <stdio.h>
#include <ming.h>

#include "ming_utils.h"

#define OUTPUT_VERSION 7
#define OUTPUT_FILENAME "DefineEditTextTest.swf"

SWFDisplayItem add_text_field(SWFMovie mo, SWFBlock font, const char* text);

SWFDisplayItem
add_text_field(SWFMovie mo, SWFBlock font, const char* text)
{
  SWFTextField tf;

  tf = newSWFTextField();

  SWFTextField_setFont(tf, font);

  /* setting flags seem unneeded */
  /*SWFTextField_setFlags(tf, SWFTEXTFIELD_USEFONT|SWFTEXTFIELD_NOEDIT);*/
  SWFTextField_addChars(tf, text);

  SWFTextField_addString(tf, text);

  /*
   * Bounds computed by Ming (if we omit the setBounds call)
   * are 2640, 240. This means that we're shrinking the available
   * space with this explicit setting. Gnash chokes in this case.
   *
   * Ref: https://savannah.gnu.org/bugs/?func=detailitem&item_id=16637.
   */
  SWFTextField_setBounds(tf, 100, 100);
  //SWFTextField_setBounds(tf, 60000, 338);

  /*
   * The following settings (found in the reported SWF)
   * are not needed to exploit the bug.
   */
 
  /*SWFTextField_setHeight(tf, 240);*/
  /*SWFTextField_setColor(tf, 0x00, 0x00, 0x00, 0xff);*/
  /*SWFTextField_setAlignment(tf, SWFTEXTFIELD_ALIGN_LEFT);*/
  /*SWFTextField_setLeftMargin(tf, 0);*/
  /*SWFTextField_setRightMargin(tf, 0);*/
  /*SWFTextField_setIndentation(tf, 0);*/
  /*SWFTextField_setLineSpacing(tf, 40);*/
  /*SWFTextField_setLineSpacing(tf, 40);*/

  return SWFMovie_add(mo, (SWFBlock)tf);
}

int
main(int argc, char** argv)
{
  SWFMovie mo;
  const char *srcdir=".";
  char fdbfont[256];
  SWFMovieClip  dejagnuclip;
  
  /*********************************************
   *
   * Initialization
   *
   *********************************************/

  if ( argc>1 ) srcdir=argv[1];
  else
  {
    fprintf(stderr, "Usage: %s <mediadir>\n", argv[0]);
    return 1;
  }

  sprintf(fdbfont, "%s/Bitstream Vera Sans.fdb", srcdir);

  puts("Setting things up");

  Ming_init();
  Ming_useSWFVersion (OUTPUT_VERSION);
  //Ming_setScale(20.0); /* so we talk twips */
 
  mo = newSWFMovie();
  SWFMovie_setRate(mo, 12);
  SWFMovie_setDimension(mo, 800, 600);
  
  dejagnuclip = get_dejagnu_clip((SWFBlock)get_default_font(srcdir), 10, 0, 0, 800, 600);
  SWFMovie_add(mo, (SWFBlock)dejagnuclip);
  SWFMovie_nextFrame(mo); // 1st frame 

  /*********************************************
   *
   * Add some textfields
   *
   *********************************************/
  {
    SWFDisplayItem it;
    FILE *font_file = fopen(fdbfont, "r");
    if ( font_file == NULL )
    {
      perror(fdbfont);
      exit(1);
    }
    SWFBrowserFont bfont = newSWFBrowserFont("_sans");
    SWFFont efont = loadSWFFontFromFile(font_file);

    it = add_text_field(mo, (SWFBlock)bfont, "Hello");
    SWFDisplayItem_setName(it, "dtext1");
    SWFDisplayItem_moveTo(it, 0, 200);
    it = add_text_field(mo, (SWFBlock)efont, "Hello");
    SWFDisplayItem_setName(it, "etext1");
    SWFDisplayItem_moveTo(it, 0, 300);

    SWFBrowserFont bfont2 = newSWFBrowserFont("times");
    it = add_text_field(mo, (SWFBlock)bfont2, "Hello");
    SWFDisplayItem_setName(it, "dtext2");
    SWFDisplayItem_moveTo(it, 0, 400);
  }
  SWFMovie_nextFrame(mo); 
  
  check_equals(mo, "typeof(dtext1)", "'object'");
  check_equals(mo, "typeof(dtext1.text)", "'string'");
  check_equals(mo, "typeof(dtext1.__proto__.text)", "'undefined'");
  

  xcheck_equals(mo, "dtext1.__proto__.hasOwnProperty('text')", "true");
  xcheck_equals(mo, "dtext1.__proto__.hasOwnProperty('background')", "true");
  xcheck_equals(mo, "dtext1.__proto__.hasOwnProperty('backgroundColor')", "true");
  
  check_equals(mo, "dtext1.text", "'Hello'");
  check_equals(mo, "etext1.text", "'Hello'");
  check_equals(mo, "dtext2.text", "'Hello'");
  xcheck_equals(mo, "dtext1.background", "false");
  xcheck_equals(mo, "etext1.background", "false");
  xcheck_equals(mo, "dtext2.background", "false");
  xcheck_equals(mo, "dtext1.backgroundColor", "0xffffff");
  xcheck_equals(mo, "etext1.backgroundColor", "0xffffff");
  xcheck_equals(mo, "dtext2.backgroundColor", "0xffffff");
  
  add_actions(mo, "dtext1.background = true;"
                  "etext1.background = true;"
                  "dtext2.background = true;"
                  "dtext1.backgroundColor = 0xff0000;"
                  "etext1.backgroundColor = 0x00ff00;"
                  "dtext2.backgroundColor = 0x0000ff;"
                  "dtext1.text += ' world';"
                  "etext1.text += ' world';"
                  "dtext2.text += ' world';" );

  check_equals(mo, "dtext1.text", "'Hello world'");
  check_equals(mo, "etext1.text", "'Hello world'");
  check_equals(mo, "dtext2.text", "'Hello world'");
  check_equals(mo, "dtext1.background", "true");
  check_equals(mo, "etext1.background", "true");
  check_equals(mo, "dtext2.background", "true");
  check_equals(mo, "dtext1.backgroundColor", "0xff0000");
  check_equals(mo, "etext1.backgroundColor", "0x00ff00");
  check_equals(mo, "dtext2.backgroundColor", "0x0000ff");
  
  add_actions(mo, "totals(); stop();");
  SWFMovie_nextFrame(mo); 
  /*****************************************************
   *
   * Output movie
   *
   *****************************************************/
  puts("Saving " OUTPUT_FILENAME );

  SWFMovie_save(mo, OUTPUT_FILENAME);

  return 0;
}
