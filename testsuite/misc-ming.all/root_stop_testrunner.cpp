/* 
 *   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#define INPUT_FILENAME "root_stop_test.swf"

#define HAVE_CONFIG_H  1

#include "MovieTester.h"
#include "sprite_instance.h"
#include "character.h"
#include "dlist.h"
#include "container.h"
#include "log.h"

#include "check.h"
#include <string>
#include <cassert>

using namespace gnash;
using namespace std;

int
main(int /*argc*/, char** /*argv*/)
{
	string filename = string(TGTDIR) + string("/") + string(INPUT_FILENAME);
	MovieTester tester(filename);

	sprite_instance* root = tester.getRootMovie();
	assert(root);

	check_equals(root->get_frame_count(), 2);
	check_equals(root->get_current_frame(), 0);
	check_equals(root->get_play_state(), sprite_instance::STOP);

	const character* ch = tester.findDisplayItemByName(*root, "mc_in_root");
	check(ch);
	const sprite_instance* mc = dynamic_cast<const sprite_instance*>(ch);
	check(mc);
	check_equals(mc->get_current_frame(), 0);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);

	tester.advance();
	check_equals(root->get_current_frame(), 0);  // we were in stop mode
	check_equals(mc->get_current_frame(), 1); 
	check_equals(root->get_play_state(), sprite_instance::STOP);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);

	tester.advance();
	check_equals(root->get_current_frame(), 0); // we were in stop mode
	check_equals(mc->get_current_frame(), 2);  
	check_equals(root->get_play_state(), sprite_instance::STOP);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);

	tester.advance();
	check_equals(root->get_current_frame(), 0); // we were in stop mode
	check_equals(mc->get_current_frame(), 0); 
	check_equals(root->get_play_state(), sprite_instance::PLAY);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);

	tester.advance();
	check_equals(root->get_current_frame(), 1); 
	check_equals(mc->get_current_frame(), 1); 
	check_equals(root->get_play_state(), sprite_instance::PLAY);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);

	tester.advance();
	check_equals(root->get_current_frame(), 0);  // looped
	check_equals(mc->get_current_frame(), 2); 
	check_equals(root->get_play_state(), sprite_instance::PLAY);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);

	tester.advance();
	check_equals(root->get_current_frame(), 1); 
	check_equals(mc->get_current_frame(), 0); 
	check_equals(root->get_play_state(), sprite_instance::PLAY);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);

	tester.advance();
	check_equals(root->get_current_frame(), 0); // looped again
	check_equals(mc->get_current_frame(), 1); 
	check_equals(root->get_play_state(), sprite_instance::PLAY);
	check_equals(mc->get_play_state(), sprite_instance::PLAY);
  	
	return 0;
}

