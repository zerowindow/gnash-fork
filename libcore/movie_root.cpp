// movie_root.cpp:  The root movie, for Gnash.
// 
//   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Free Software
//   Foundation, Inc
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

#include "GnashSystemIOHeaders.h" // write()

#include "movie_root.h"
#include "log.h"
#include "MovieClip.h"
#include "Movie.h" // for implicit upcast to MovieClip
#include "VM.h"
#include "ExecutableCode.h"
#include "URL.h"
#include "namedStrings.h"
#include "GnashException.h"
#include "sound_handler.h"
#include "Timers.h"
#include "GnashKey.h"
#include "MovieFactory.h"
#include "GnashAlgorithm.h"
#include "GnashNumeric.h"
#include "Global_as.h"
#include "utf8.h"
#include "IOChannel.h"
#include "RunResources.h"
#include "Renderer.h"

#include <boost/algorithm/string/replace.hpp>
#include <utility>
#include <iostream>
#include <string>
#include <map>
#include <bitset>
#include <typeinfo>
#include <cassert>
#include <functional> // std::bind2nd, std::equal_to
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/bind.hpp>

#ifdef USE_SWFTREE
# include "tree.hh"
#endif

//#define GNASH_DEBUG 1
//#define GNASH_DEBUG_LOADMOVIE_REQUESTS_PROCESSING 1
//#define GNASH_DEBUG_TIMERS_EXPIRATION 1

// Defining the macro below prints info about
// cleanup of live chars (advanceable + key/mouse listeners)
// Is useful in particular to check for cost of multiple scans
// when a movie destruction destrois more elements.
//
// NOTE: I think the whole confusion here was introduced
//       by zou making it "optional" to ::unload() childs
//       when being unloaded. Zou was trying to avoid
//       queuing an onUnload event, which I suggested we'd
//       do by having unload() take an additional argument
//       or similar. Failing to tag childs as unloaded
//       will result in tagging them later (in ::destroy)
//       which will require scanning the lists again
//       (key/mouse + advanceable).
//       See https://savannah.gnu.org/bugs/index.php?21804
//
//#define GNASH_DEBUG_DLIST_CLEANUP 1

namespace gnash {

namespace {
    bool generate_mouse_button_events(movie_root& mr, MouseButtonState& ms);
    const DisplayObject* getNearestObject(const DisplayObject* o);
    as_object* getBuiltinObject(movie_root& mr, string_table::key cl);
}

}

namespace gnash {

inline bool
movie_root::testInvariant() const
{
    // TODO: fill this function !
    // The _movies map can not invariantably
    // be non-empty as the stage is autonomous
    // itself
    //assert( ! _movies.empty() );

    return true;
}


movie_root::movie_root(const movie_definition& def,
        VirtualClock& clock, const RunResources& runResources)
    :
    _runResources(runResources),
    _originalURL(def.get_url()),
    _vm(VM::init(def.get_version(), *this, clock)),
    _interfaceHandler(0),
    _fsCommandHandler(0),
    m_viewport_x0(0),
    m_viewport_y0(0),
    m_viewport_width(1),
    m_viewport_height(1),
    m_background_color(255, 255, 255, 255),
    m_background_color_set(false),
    m_timer(0.0f),
    m_mouse_x(0),
    m_mouse_y(0),
    m_mouse_buttons(0),
    _lastTimerId(0),
    _lastKeyEvent(key::INVALID),
    _currentFocus(0),
    m_drag_state(),
    _movies(),
    _rootMovie(0),
    _invalidated(true),
    _disableScripts(false),
    _processingActionLevel(PRIORITY_SIZE),
    _hostfd(-1),
    _controlfd(-1),
    _quality(QUALITY_HIGH),
    _alignMode(0),
    _allowScriptAccess(SCRIPT_ACCESS_SAME_DOMAIN),
    _showMenu(true),
    _scaleMode(SCALEMODE_SHOWALL),
    _displayState(DISPLAYSTATE_NORMAL),
    _recursionLimit(256),
    _timeoutLimit(15),
    _movieAdvancementDelay(83), // ~12 fps by default
    _lastMovieAdvancement(0),
    _unnamedInstance(0),
    _movieLoader(*this)
{
    // This takes care of informing the renderer (if present) too.
    setQuality(QUALITY_HIGH);
}

void
movie_root::disableScripts()
{
    _disableScripts = true;

    // NOTE: we won't clear the action queue now
    // to avoid invalidating iterators as we've
    // been probably called during processing
    // of the queue.
}

    
size_t
movie_root::nextUnnamedInstance()
{
    return ++_unnamedInstance;
}

void
movie_root::clearActionQueue()
{
    for (int lvl=0; lvl < PRIORITY_SIZE; ++lvl) {
        ActionQueue& q = _actionQueue[lvl];
        deleteChecked(q.begin(), q.end());
        q.clear();
    }
}

void
movie_root::clearIntervalTimers()
{
    deleteSecondElements(_intervalTimers.begin(), _intervalTimers.end());
    _intervalTimers.clear();
}

movie_root::~movie_root()
{
    clearActionQueue();
    clearIntervalTimers();
    _movieLoader.clear();

    assert(testInvariant());
}

Movie*
movie_root::init(movie_definition* def, const MovieClip::MovieVariables& vars,
                 const MovieClip::MovieVariables& scriptables)
{
    Movie* mr = def->createMovie(*_vm.getGlobal());
    mr->setVariables(vars);
    mr->setVariables(scriptables);
    setRootMovie(mr);
    return mr;
}

void
movie_root::setRootMovie(Movie* movie)
{
    _rootMovie = movie;

    m_viewport_x0 = 0;
    m_viewport_y0 = 0;
    const movie_definition* md = movie->definition();
    float fps = md->get_frame_rate();
    _movieAdvancementDelay = static_cast<int>(1000/fps);

    _lastMovieAdvancement = _vm.getTime();

    m_viewport_width = static_cast<int>(md->get_width_pixels());
    m_viewport_height = static_cast<int>(md->get_height_pixels());

    // assert(movie->get_depth() == 0); ?
    movie->set_depth(DisplayObject::staticDepthOffset);

    try
    {
        setLevel(0, movie);

        // actions in first frame of _level0 must execute now,
        // before next advance,
        // or they'll be executed with _currentframe being set to 2
        processActionQueue();
    }
    catch (ActionLimitException& al)
    {
        boost::format fmt = boost::format(_("ActionLimits hit during "
                    "setRootMovie: %s. Disable scripts?")) % al.what();
        handleActionLimitHit(fmt.str());
    }
    catch (ActionParserException& e)
    {
        log_error("ActionParserException thrown during setRootMovie: %s",
                e.what());
    }

    cleanupAndCollect();
}

/*private*/
void
movie_root::handleActionLimitHit(const std::string& msg)
{
    bool disable = true;
    if ( _interfaceHandler ) disable = _interfaceHandler->yesNo(msg);
    else log_error("No user interface registered, assuming 'Yes' answer to "
            "question: %s", msg);
    if ( disable )
    {
        disableScripts();
        clearActionQueue();
    }
}

void
movie_root::cleanupAndCollect()
{
    // Cleanup the stack.
    _vm.getStack().clear();

    cleanupUnloadedListeners();
    cleanupDisplayList();
    GC::get().fuzzyCollect();
}

/* private */
void
movie_root::setLevel(unsigned int num, Movie* movie)
{
    assert(movie != NULL);
    assert(static_cast<unsigned int>(movie->get_depth()) ==
                            num + DisplayObject::staticDepthOffset);


    Levels::iterator it = _movies.find(movie->get_depth());
    if ( it == _movies.end() )
    {
        _movies[movie->get_depth()] = movie; 
    }
    else
    {
        // don't leak overloaded levels

        LevelMovie lm = it->second;
        if (lm == _rootMovie)
        {
            // NOTE: this is not enough to trigger
            //       an application reset. Was tested
            //       but not automated. If curious
            //       use swapDepths against _level0
            //       and load into the new target while
            //       a timeout/interval is active.
            log_debug("Replacing starting movie");
        }

        if ( num == 0 )
        {
            log_debug("Loading into _level0");

            // NOTE: this was tested but not automated, the
            //       test sets an interval and then loads something
            //       in _level0. The result is the interval is disabled.
            clearIntervalTimers();


            // TODO: check what else we should do in these cases 
            //       (like, unregistering all childs etc...)
            //       Tested, but not automated, is that other
            //       levels should be maintained alive.
            // Sat Nov 14 10:31:19 CET 2009
            // ^^^ not confirmed in this date, I think other levels 
            //     are dropped too! (strk)

            m_viewport_width = static_cast<int>(movie->widthPixels());
            m_viewport_height = static_cast<int>(movie->heightPixels());

            // notify  stage replacement
            if ( _interfaceHandler )
            {
                std::stringstream ss;
                ss << m_viewport_width << "x" << m_viewport_height;
                _interfaceHandler->call("Stage.resize", ss.str());
            }
        }

        it->second->destroy();
        it->second = movie;
    }

    movie->set_invalidated();
    
    /// Notify placement 
    movie->stagePlacementCallback();

    assert(testInvariant());
}

void
movie_root::swapLevels(MovieClip* movie, int depth)
{
    assert(movie);

//#define GNASH_DEBUG_LEVELS_SWAPPING 1

    int oldDepth = movie->get_depth();

#ifdef GNASH_DEBUG_LEVELS_SWAPPING
    log_debug("Before swapLevels (source depth %d, target depth %d) "
            "levels are: ", oldDepth, depth);
    for (Levels::const_iterator i=_movies.begin(), e=_movies.end(); i!=e; ++i)
    {
        log_debug(" %d: %p (%s @ depth %d)", i->first,
                (void*)(i->second), i->second->getTarget(),
                i->second->get_depth());
    }
#endif

    if ( oldDepth < DisplayObject::staticDepthOffset ) // should include _level0 !
    {
        IF_VERBOSE_ASCODING_ERRORS(
        log_aserror(_("%s.swapDepth(%d): movie has a depth (%d) below "
                "static depth zone (%d), won't swap its depth"),
                movie->getTarget(), depth, oldDepth,
                DisplayObject::staticDepthOffset);
        );
        return;
    }

    if ( oldDepth >= 0 ) 
    {
        IF_VERBOSE_ASCODING_ERRORS(
        log_aserror(_("%s.swapDepth(%d): movie has a depth (%d) below "
                "static depth zone (%d), won't swap its depth"),
                movie->getTarget(), depth, oldDepth,
                DisplayObject::staticDepthOffset);
        );
        return;
    }

    int oldNum = oldDepth; // -DisplayObject::staticDepthOffset;
    Levels::iterator oldIt = _movies.find(oldNum);
    if ( oldIt == _movies.end() )
    {
        log_debug("%s.swapDepth(%d): target depth (%d) contains no movie",
            movie->getTarget(), depth, oldNum);
        return;
    }

    int newNum = depth; // -DisplayObject::staticDepthOffset;
    movie->set_depth(depth);
    Levels::iterator targetIt = _movies.find(newNum);
    if ( targetIt == _movies.end() )
    {
        _movies.erase(oldIt);
        _movies[newNum] = movie;
    }
    else
    {
        MovieClip* otherMovie = targetIt->second;
        otherMovie->set_depth(oldDepth);
        oldIt->second = otherMovie;
        targetIt->second = movie;
    }
    
#ifdef GNASH_DEBUG_LEVELS_SWAPPING
    log_debug("After swapLevels levels are: ");
    for (Levels::const_iterator i=_movies.begin(), e=_movies.end(); i!=e; ++i)
    {
        log_debug(" %d: %p (%s @ depth %d)", i->first, 
                (void*)(i->second), i->second->getTarget(),
                i->second->get_depth());
    }
#endif
    
    // TODO: invalidate self, not the movie
    //       movie_root::setInvalidated() seems
    //       to do just that, if anyone feels
    //       like more closely research on this
    //       (does level swapping require full redraw always?)
    movie->set_invalidated();
    
    assert(testInvariant());
}

void
movie_root::dropLevel(int depth)
{
    // should be checked by caller
    assert ( depth >= 0 && depth <= 1048575 );

    Levels::iterator it = _movies.find(depth);
    if ( it == _movies.end() )
    {
        log_error("movie_root::dropLevel called against a movie not "
                "found in the levels container");
        return;
    }

    MovieClip* mo = it->second;
    if (mo == _rootMovie)
    {
        IF_VERBOSE_ASCODING_ERRORS(
        log_aserror(_("Original root movie can't be removed"));
        );
        return;
    }

    // TOCHECK: safe to erase here ?
    mo->unload();
    mo->destroy();
    _movies.erase(it);

    assert(testInvariant());
}

void
movie_root::replaceLevel(unsigned int num, Movie* extern_movie)
{
    extern_movie->set_depth(num + DisplayObject::staticDepthOffset);
    Levels::iterator it = _movies.find(extern_movie->get_depth());
    if ( it == _movies.end() )
    {
        log_error("TESTME: loadMovie called on level %d which is not available at load time, skipped placement for now");
        return; // skip
    }

    // TODO: rework this to avoid the double scan 
    setLevel(num, extern_movie);
}

MovieClip*
movie_root::getLevel(unsigned int num) const
{
    Levels::const_iterator i =
        _movies.find(num + DisplayObject::staticDepthOffset);

    if (i == _movies.end()) return 0;

    return i->second;
}

void
movie_root::reset()
{
    sound::sound_handler* sh = _runResources.soundHandler();
    if ( sh ) sh->reset();
    clear();
    _disableScripts = false;
}

void
movie_root::clear()
{
    // reset background color, to allow 
    // next load to set it again.
    m_background_color.set(255,255,255,255);
    m_background_color_set = false;

    // wipe out live chars
    _liveChars.clear();

    // wipe out queued actions
    clearActionQueue();

    // wipe out all levels
    _movies.clear();

    // remove all intervals
    clearIntervalTimers();

    // remove all loadMovie requests
    _movieLoader.clear();

    // remove key/mouse listeners
    _keyListeners.clear();
    _mouseListeners.clear();

    // Cleanup the stack.
    _vm.getStack().clear();

#ifdef GNASH_USE_GC
    // Run the garbage collector again
    GC::get().fuzzyCollect();
#endif

    setInvalidated();
}

void
movie_root::set_display_viewport(int x0, int y0, int w, int h)
{
    assert(testInvariant());

    m_viewport_x0 = x0;
    m_viewport_y0 = y0;
    m_viewport_width = w;
    m_viewport_height = h;

    if (_scaleMode == SCALEMODE_NOSCALE) {
        //log_debug("Rescaling disabled");
        as_object* stage = getBuiltinObject(*this, NSV::PROP_iSTAGE);
        if (stage) {
            log_debug("notifying Stage listeners about a resize");
            callMethod(stage, NSV::PROP_BROADCAST_MESSAGE, "onResize");
        }

    }

    assert(testInvariant());
}

bool
movie_root::notify_mouse_moved(int x, int y)
{
    assert(testInvariant());

    m_mouse_x = x;
    m_mouse_y = y;
    notify_mouse_listeners(event_id::MOUSE_MOVE);
    return fire_mouse_event();

}


bool
movie_root::notify_key_event(key::code k, bool down)
{
    _lastKeyEvent = k;
    const size_t keycode = key::codeMap[k][key::KEY];
    if (keycode < key::KEYCOUNT) {
        _unreleasedKeys.set(keycode, down);
    }

    // Notify DisplayObject key listeners for clip key events
    notify_key_listeners(k, down);

    // Notify both DisplayObject and non-DisplayObject Key listeners
    //    for user defined handers.
    as_object* key = getBuiltinObject(*this, NSV::CLASS_KEY);
    if (key) {

        try {
            // Can throw an action limit exception if the stack limit is 0 or 1,
            // i.e. if the stack is at the limit before it contains anything.
            // A stack limit like that is hardly of any use, but could be used
            // maliciously to crash Gnash.
            if (down) {
                callMethod(key, NSV::PROP_BROADCAST_MESSAGE, "onKeyDown");
            }
            else {
                callMethod(key, NSV::PROP_BROADCAST_MESSAGE, "onKeyUp");
            }
        }
        catch (ActionLimitException &e)
        {
            log_error(_("ActionLimits hit notifying key listeners: %s."),
                    e.what());
            clearActionQueue();
        }
    }

    processActionQueue();

    return false; // should return true if needs update ...
}


bool
movie_root::notify_mouse_clicked(bool mouse_pressed, int button_mask)
{
    assert(testInvariant());

    //log_debug("Mouse click notification");
    if (mouse_pressed)
    {
        m_mouse_buttons |= button_mask;
        notify_mouse_listeners(event_id(event_id::MOUSE_DOWN));
    }
    else
    {
        m_mouse_buttons &= ~button_mask;
        notify_mouse_listeners(event_id(event_id::MOUSE_UP));
    }

    return fire_mouse_event();
}


bool
movie_root::fire_mouse_event()
{
//    GNASH_REPORT_FUNCTION;

    assert(testInvariant());

    boost::int32_t x = pixelsToTwips(m_mouse_x);
    boost::int32_t y = pixelsToTwips(m_mouse_y);

    // Generate a mouse event
    _mouseButtonState.topmostEntity = getTopmostMouseEntity(x, y);
    _mouseButtonState.currentButtonState = (m_mouse_buttons & 1);

    // Set _droptarget if dragging a sprite
    MovieClip* dragging = 0;
    DisplayObject* draggingChar = getDraggingCharacter();
    if (draggingChar) dragging = draggingChar->to_movie();
    if (dragging)
    {
        // TODO: optimize making findDropTarget and getTopmostMouseEntity
        //       use a single scan.
        const DisplayObject* dropChar = findDropTarget(x, y, dragging);
        if (dropChar)
        {
            // Use target of closest script DisplayObject containing this
            dropChar = getNearestObject(dropChar);
            dragging->setDropTarget(dropChar->getTargetPath());
        }
        else dragging->setDropTarget("");

    }

    bool need_redraw = false;

    // FIXME: need_redraw might also depend on actual
    //        actions execution (consider updateAfterEvent).

    try
    {
        need_redraw = generate_mouse_button_events(*this, _mouseButtonState);
        processActionQueue();
    }
    catch (ActionLimitException& al)
    {
        boost::format fmt = boost::format(_("ActionLimits hit during mouse event processing: %s. Disable scripts ?")) % al.what();
        handleActionLimitHit(fmt.str());
    }

    return need_redraw;

}

void
movie_root::get_mouse_state(boost::int32_t& x, boost::int32_t& y,
        boost::int32_t& buttons)
{

    assert(testInvariant());

    x = m_mouse_x;
    y = m_mouse_y;
    buttons = m_mouse_buttons;

    assert(testInvariant());
}

void
movie_root::get_drag_state(drag_state& st)
{
    assert(testInvariant());

    st = m_drag_state;

    assert(testInvariant());
}

void
movie_root::set_drag_state(const drag_state& st)
{
    m_drag_state = st;
    DisplayObject* ch = st.getCharacter();
    if ( ch && ! st.isLockCentered() )
    {
        // Get coordinates of the DisplayObject's origin
        point origin(0, 0);
        SWFMatrix chmat = ch->getWorldMatrix();
        point world_origin;
        chmat.transform(&world_origin, origin);

        // Get current mouse coordinates
        boost::int32_t x, y, buttons;
        get_mouse_state(x, y, buttons);
        point world_mouse(pixelsToTwips(x), pixelsToTwips(y));

        boost::int32_t xoffset = world_mouse.x - world_origin.x;
        boost::int32_t yoffset = world_mouse.y - world_origin.y;

        m_drag_state.setOffset(xoffset, yoffset);
    }
    assert(testInvariant());
}

void
movie_root::doMouseDrag()
{
    DisplayObject* dragChar = getDraggingCharacter(); 
    if ( ! dragChar ) return; // nothing to do

    if ( dragChar->unloaded() )
    {
        // Reset drag state if dragging char was unloaded
        m_drag_state.reset();
        return; 
    }

    boost::int32_t x, y, buttons;
    get_mouse_state(x, y, buttons);

    point world_mouse(pixelsToTwips(x), pixelsToTwips(y));

    SWFMatrix    parent_world_mat;
    DisplayObject* parent = dragChar->get_parent();
    if (parent != NULL)
    {
        parent_world_mat = parent->getWorldMatrix();
    }

    if (! m_drag_state.isLockCentered())
    {
        world_mouse.x -= m_drag_state.xOffset();
        world_mouse.y -= m_drag_state.yOffset();
    }

    if ( m_drag_state.hasBounds() )
    {
        SWFRect bounds;
        // bounds are in local coordinate space
        bounds.enclose_transformed_rect(parent_world_mat, m_drag_state.getBounds());
        // Clamp mouse coords within a defined SWFRect.
        bounds.clamp(world_mouse);
    }

    parent_world_mat.invert().transform(world_mouse);            
    // Place our origin so that it coincides with the mouse coords
    // in our parent frame.
    // TODO: add a DisplayObject::set_translation ?
    SWFMatrix    local = dragChar->getMatrix();
    local.set_translation(world_mouse.x, world_mouse.y);
    dragChar->setMatrix(local); //no need to update caches when only changing translation
}


unsigned int
movie_root::add_interval_timer(std::auto_ptr<Timer> timer)
{
    assert(timer.get());
    assert(testInvariant());
            
    int id = ++_lastTimerId;

    assert(_intervalTimers.find(id) == _intervalTimers.end());
    _intervalTimers[id] = timer.release(); 
    return id;
}
    
bool
movie_root::clear_interval_timer(unsigned int x)
{
    TimerMap::iterator it = _intervalTimers.find(x);
    if ( it == _intervalTimers.end() ) return false;

    // We do not remove the element here because
    // we might have been called during execution
    // of another timer, thus during a scan of the _intervalTimers
    // container. If we use erase() here, the iterators in executeTimers
    // would be invalidated. Rather, executeTimers() would check container
    // elements for being still active and remove the cleared one in a safe way
    // at each iteration.
    it->second->clearInterval();

    return true;

}

bool
movie_root::advance()
{
    // We can't actually rely on now being later than _lastMovieAdvancement,
    // so we will have to check. Otherwise we risk elapsed being
    // contructed from a negative value.
    const size_t now = std::max<size_t>(_vm.getTime(), _lastMovieAdvancement);

    bool advanced = false;

    try {

        const size_t elapsed = now - _lastMovieAdvancement;
        if (elapsed >= _movieAdvancementDelay)
        {
            advanced = true;
            advanceMovie();

            // To catch-up lateness we pretend we advanced when 
            // was time for it. 
            // NOTE:
            //   now - _lastMovieAdvancement
            // gives you actual lateness in milliseconds
            //
            _lastMovieAdvancement += _movieAdvancementDelay;

        }

        //log_debug("Latenss: %d", now-_lastMovieAdvancement);
        
        executeAdvanceCallbacks();
        
        executeTimers();
    
    }
    catch (ActionLimitException& al) {
        // The PP does not disable scripts when the stack limit is reached,
        // but rather struggles on. 
        log_error(_("Action limit hit during advance: %s"), al.what());
        clearActionQueue();
    }
    catch (ActionParserException& e) {
        log_error(_("Buffer overread during advance: %s"), e.what());
        clearActionQueue();
    }

    return advanced;
}
    
void
movie_root::advanceMovie()
{

    // Do mouse drag, if needed
    doMouseDrag();

    // Advance all non-unloaded DisplayObjects in the LiveChars list
    // in reverse order (last added, first advanced)
    // NOTE: can throw ActionLimitException
    advanceLiveChars(); 

    // Process loadMovie requests
    // 
    // NOTE: should be done before executing timers,
    //      see swfdec's test/trace/loadmovie-case-{5,6}.swf 
    // NOTE: processing loadMovie requests after advanceLiveChars
    //       is known to fix more tests in misc-mtasc.all/levels.swf
    //       to be checked if it keeps the swfdec testsuite safe
    //
    _movieLoader.processCompletedRequests();

    // Process queued actions
    // NOTE: can throw ActionLimitException
    processActionQueue();

    cleanupAndCollect();

    assert(testInvariant());
}

int
movie_root::timeToNextFrame() const
{
    unsigned int now = _vm.getTime();
    const int elapsed = now - _lastMovieAdvancement;
    return _movieAdvancementDelay - elapsed;
}

void
movie_root::display()
{
//    GNASH_REPORT_FUNCTION;

    assert(testInvariant());

    clearInvalidated();

    // TODO: should we consider the union of all levels bounds ?
    const SWFRect& frame_size = _rootMovie->get_frame_size();
    if ( frame_size.is_null() )
    {
        // TODO: check what we should do if other levels
        //       have valid bounds
        log_debug("original root movie had null bounds, not displaying");
        return;
    }

    Renderer* renderer = _runResources.renderer();
    if (!renderer) return;

    renderer->begin_display(
        m_background_color,
        m_viewport_x0, m_viewport_y0,
        m_viewport_width, m_viewport_height,
        frame_size.get_x_min(), frame_size.get_x_max(),
        frame_size.get_y_min(), frame_size.get_y_max());


    for (Levels::iterator i=_movies.begin(), e=_movies.end(); i!=e; ++i)
    {
        MovieClip* movie = i->second;

        movie->clear_invalidated();

        if (movie->visible() == false) continue;

        // null frame size ? don't display !
        const SWFRect& sub_frame_size = movie->get_frame_size();

        if ( sub_frame_size.is_null() )
        {
            log_debug("_level%u has null frame size, skipping", i->first);
            continue;
        }

        movie->display(*renderer);

    }

    renderer->end_display();
}



void movie_root::cleanupUnloadedListeners(Listeners& ll)
{
    bool needScan;

#ifdef GNASH_DEBUG_DLIST_CLEANUP
    int scansCount = 0;
#endif

    do
    {

#ifdef GNASH_DEBUG_DLIST_CLEANUP
      scansCount++;
      int cleaned =0;
#endif

      needScan=false;

      // remove unloaded DisplayObject listeners from movie_root
      for (Listeners::iterator iter = ll.begin(); iter != ll.end(); )
      {
          DisplayObject* const ch = *iter;
          if ( ch->unloaded() )
          {
            if ( ! ch->isDestroyed() )
            {
              ch->destroy();
              needScan=true; // ->destroy() might mark already-scanned chars as unloaded
            }
            iter = ll.erase(iter);

#ifdef GNASH_DEBUG_DLIST_CLEANUP
            cleaned++;
#endif

          }

          else ++iter;
      }

#ifdef GNASH_DEBUG_DLIST_CLEANUP
      cout << " Scan " << scansCount << " cleaned " << cleaned << " instances" << endl;
#endif

    } while (needScan);
    
}

void
movie_root::notify_key_listeners(key::code k, bool down)
{

    Listeners copy = _keyListeners;
    for (Listeners::iterator iter = copy.begin(), itEnd=copy.end();
            iter != itEnd; ++iter)
    {
        // sprite, button & input_edit_text DisplayObjects
        DisplayObject* const ch = *iter;
        if (!ch->unloaded()) {
            if (down) {
                // KEY_UP and KEY_DOWN events are unrelated to any key!
                ch->notifyEvent(event_id(event_id::KEY_DOWN, key::INVALID)); 
                // Pass the unique Gnash key code!
                ch->notifyEvent(event_id(event_id::KEY_PRESS, k));
            }
            else {
                ch->notifyEvent(event_id(event_id::KEY_UP, key::INVALID));   
            }
        }
    }

    assert(testInvariant());

    if (!copy.empty()) {
        // process actions queued in the above step
        processActionQueue();
    }
}

void
movie_root::add_listener(Listeners& ll, DisplayObject* listener)
{
    assert(listener);

    // Don't add the same listener twice (why not use a set?)
    if (std::find(ll.begin(), ll.end(), listener) != ll.end()) return;

    ll.push_front(listener);
}


void
movie_root::remove_listener(Listeners& ll, DisplayObject* listener)
{
    assert(listener);
    ll.remove_if(std::bind2nd(std::equal_to<DisplayObject*>(), listener));
}

void
movie_root::notify_mouse_listeners(const event_id& event)
{

    Listeners copy = _mouseListeners;
    for (Listeners::iterator iter = copy.begin(), itEnd=copy.end();
            iter != itEnd; ++iter)
    {
        DisplayObject* const ch = *iter;
        if (!ch->unloaded())
        {
            ch->notifyEvent(event);
        }
    }

    as_object* mouseObj = getBuiltinObject(*this, NSV::CLASS_MOUSE);
    if (mouseObj) {

        try {
            // Can throw an action limit exception if the stack limit is 0 or 1.
            // A stack limit like that is hardly of any use, but could be used
            // maliciously to crash Gnash.
            callMethod(mouseObj, NSV::PROP_BROADCAST_MESSAGE, 
                    event.functionName());
        }
        catch (ActionLimitException &e) {
            log_error(_("ActionLimits hit notifying mouse events: %s."),
                    e.what());
            clearActionQueue();
        }
        
    }

    assert(testInvariant());

    if (!copy.empty()) {
        // process actions queued in the above step
        processActionQueue();
    }
}

DisplayObject*
movie_root::getFocus()
{
    assert(testInvariant());
    return _currentFocus;
}

bool
movie_root::setFocus(DisplayObject* to)
{

    // Nothing to do if current focus is the same as the new focus. 
    // _level0 also seems unable to receive focus under any circumstances
    // TODO: what about _level1 etc ?
    if (to == _currentFocus ||
            to == static_cast<DisplayObject*>(_rootMovie)) {
        return false;
    }

    if (to && !to->handleFocus()) {
        // TODO: not clear whether to remove focus in this case.
        return false;
    }

    // Undefined or NULL DisplayObject removes current focus. Otherwise, try
    // setting focus to the new DisplayObject. If it fails, remove current
    // focus anyway.

    // Store previous focus, as the focus needs to change before onSetFocus
    // is called and listeners are notified.
    DisplayObject* from = _currentFocus;

    if (from) {
        // Perform any actions required on killing focus (only TextField).
        from->killFocus();

        /// A valid focus must have an associated object.
        assert(getObject(from));
        callMethod(getObject(from), NSV::PROP_ON_KILL_FOCUS, getObject(to));
    }

    _currentFocus = to;

    if (to) {
        assert(getObject(to));
        callMethod(getObject(to), NSV::PROP_ON_SET_FOCUS, getObject(from));
    }

    as_object* sel = getBuiltinObject(*this, NSV::CLASS_SELECTION);

    // Notify Selection listeners with previous and new focus as arguments.
    // Either argument may be null.
    if (sel) {
        callMethod(sel, NSV::PROP_BROADCAST_MESSAGE, "onSetFocus",
                getObject(from), getObject(to));
    }

    assert(testInvariant());

    return true;
}

DisplayObject*
movie_root::getActiveEntityUnderPointer() const
{
    return _mouseButtonState.activeEntity;
}

DisplayObject*
movie_root::getDraggingCharacter() const
{
    return m_drag_state.getCharacter();
}

const DisplayObject*
movie_root::getEntityUnderPointer() const
{
    boost::int32_t x = pixelsToTwips(m_mouse_x);
    boost::int32_t y = pixelsToTwips(m_mouse_y);
    const DisplayObject* dropChar = findDropTarget(x, y, getDraggingCharacter()); 
    return dropChar;
}


bool
movie_root::isMouseOverActiveEntity() const
{
    assert(testInvariant());
    return (_mouseButtonState.activeEntity);
}

void
movie_root::setQuality(Quality q)
{
    gnash::RcInitFile& rcfile = gnash::RcInitFile::getDefaultInstance();

    /// Overridden quality if not negative.
    if (rcfile.qualityLevel() >= 0) {
        int ql = rcfile.qualityLevel();
        ql = std::min<int>(ql, QUALITY_BEST);
        q = static_cast<Quality>(ql);
    }

    if ( _quality != q )
    {
        // Force a redraw if quality changes
        //
        // redraw should only happen on next
        // frame advancement (tested)
        //
        setInvalidated();

        _quality = q;
    }

    // We always tell the renderer, because it could
    // be the first time we do
    Renderer* renderer = _runResources.renderer();
    if (renderer) renderer->setQuality(_quality);

}

/// Get actionscript width of stage, in pixels. The width
/// returned depends on the scale mode.
unsigned int
movie_root::getStageWidth() const
{
    if (_scaleMode == SCALEMODE_NOSCALE) {
        return m_viewport_width;    
    }

    // If scaling is allowed, always return the original movie size.
    return static_cast<unsigned int>(_rootMovie->widthPixels());
}

/// Get actionscript height of stage, in pixels. The height
/// returned depends on the scale mode.
unsigned int
movie_root::getStageHeight() const
{
    if (_scaleMode == SCALEMODE_NOSCALE) {
        return m_viewport_height;    
    }

    // If scaling is allowed, always return the original movie size.
    return static_cast<unsigned int>(_rootMovie->heightPixels());
}

/// Takes a short int bitfield: the four bits correspond
/// to the AlignMode enum 
void
movie_root::setStageAlignment(short s)
{
    _alignMode = s;
    callInterface("Stage.align");
}

/// The mode is one of never, always, with sameDomain the default
void
movie_root::setAllowScriptAccess(AllowScriptAccessMode mode)
{
    _allowScriptAccess = mode;
}

movie_root::AllowScriptAccessMode
movie_root::getAllowScriptAccess()
{
    return _allowScriptAccess;
}

/// Returns a pair of enum values giving the actual alignment
/// of the stage after align mode flags are evaluated.
movie_root::StageAlign
movie_root::getStageAlignment() const
{
    /// L takes precedence over R. Default is centred.
    StageHorizontalAlign ha = STAGE_H_ALIGN_C;
    if (_alignMode.test(STAGE_ALIGN_L)) ha = STAGE_H_ALIGN_L;
    else if (_alignMode.test(STAGE_ALIGN_R)) ha = STAGE_H_ALIGN_R;

    /// T takes precedence over B. Default is centred.
    StageVerticalAlign va = STAGE_V_ALIGN_C;
    if (_alignMode.test(STAGE_ALIGN_T)) va = STAGE_V_ALIGN_T;
    else if (_alignMode.test(STAGE_ALIGN_B)) va = STAGE_V_ALIGN_B;

    return std::make_pair(ha, va);
}

/// Returns a string that represents the boolean state of the _showMenu
/// variable
bool
movie_root::getShowMenuState() const
{
    return _showMenu;
}

/// Sets the value of _showMenu and calls the gui handler to process the 
/// fscommand to change the display of the context menu
void
movie_root::setShowMenuState(bool state)
{
    _showMenu = state;
    //FIXME: The gui code for show menu is semantically different than what
    //   ActionScript expects it to be. In gtk.cpp the showMenu function hides
    //   or shows the menubar. Flash expects this option to disable some 
    //   context menu items.
    // callInterface is the proper handler for this
    callInterface("Stage.showMenu", (_showMenu) ? "true" : "false");  //or this?
}

/// Returns the string representation of the current align mode,
/// which must always be in the order: LTRB
std::string
movie_root::getStageAlignMode() const
{
    std::string align;
    if (_alignMode.test(STAGE_ALIGN_L)) align.push_back('L');
    if (_alignMode.test(STAGE_ALIGN_T)) align.push_back('T');
    if (_alignMode.test(STAGE_ALIGN_R)) align.push_back('R');
    if (_alignMode.test(STAGE_ALIGN_B)) align.push_back('B');
    
    return align;
}

void
movie_root::setStageScaleMode(ScaleMode sm)
{
    if (_scaleMode == sm) return; // nothing to do

    bool notifyResize = false;
    
    // If we go from or to noScale, we notify a resize
    // if and only if display viewport is != then actual
    // movie size. If there is not yet a _rootMovie (when scaleMode
    // is passed as a parameter to the player), we also don't notify a 
    // resize.
    if (_rootMovie && 
            (sm == SCALEMODE_NOSCALE || _scaleMode == SCALEMODE_NOSCALE)) {

        const movie_definition* md = _rootMovie->definition();
        log_debug("Going to or from scaleMode=noScale. Viewport:%dx%d "
                "Def:%dx%d", m_viewport_width, m_viewport_height,
                md->get_width_pixels(), md->get_height_pixels());

        if ( m_viewport_width != md->get_width_pixels()
             || m_viewport_height != md->get_height_pixels() )
        {
            notifyResize = true;
        }
    }

    _scaleMode = sm;
    callInterface("Stage.align");    

    if (notifyResize) {
        as_object* stage = getBuiltinObject(*this, NSV::PROP_iSTAGE);
        if (stage) {
            log_debug("notifying Stage listeners about a resize");
            callMethod(stage, NSV::PROP_BROADCAST_MESSAGE, "onResize");
        }
    }
}

void
movie_root::setStageDisplayState(const DisplayState ds)
{
    _displayState = ds;

    as_object* stage = getBuiltinObject(*this, NSV::PROP_iSTAGE);
    if (stage) {
        log_debug("notifying Stage listeners about fullscreen state");
        const bool fs = _displayState == DISPLAYSTATE_FULLSCREEN;
        callMethod(stage, NSV::PROP_BROADCAST_MESSAGE, "onFullScreen", fs);
    }

    if (!_interfaceHandler) return; // No registered callback
    
    switch (_displayState)
    {
        case DISPLAYSTATE_FULLSCREEN:
            callInterface("Stage.displayState", "fullScreen");
            break;
        case DISPLAYSTATE_NORMAL:
            callInterface("Stage.displayState", "normal");
            break;
    }   
}

void
movie_root::add_invalidated_bounds(InvalidatedRanges& ranges, bool force)
{
    if (isInvalidated()) {
        ranges.setWorld();
        return;
    }

    for (Levels::reverse_iterator i=_movies.rbegin(), e=_movies.rend(); i!=e;
                        ++i) {
        i->second->add_invalidated_bounds(ranges, force);
    }

}

size_t
movie_root::minPopulatedPriorityQueue() const
{
    for (size_t l = 0; l < PRIORITY_SIZE; ++l) {
        if (!_actionQueue[l].empty()) return l;
    }
    return PRIORITY_SIZE;
}

size_t
movie_root::processActionQueue(size_t lvl)
{
    ActionQueue& q = _actionQueue[lvl];

    assert( minPopulatedPriorityQueue() == lvl );

#ifdef GNASH_DEBUG
    static unsigned calls=0;
    ++calls;
    bool actionsToProcess = !q.empty();
    if (actionsToProcess) {
        log_debug(" Processing %d actions in priority queue %d (call %u)",
                    q.size(), lvl, calls);
    }
#endif

    // _actionQueue may be changed due to actions (appended-to)
    // this loop might be optimized by using an iterator
    // and a final call to .clear() 
    while (!q.empty()) {

        std::auto_ptr<ExecutableCode> code(q.front());
        q.pop_front(); 
        code->execute();

        size_t minLevel = minPopulatedPriorityQueue();
        if (minLevel < lvl) {
#ifdef GNASH_DEBUG
            log_debug(" Actions pushed in priority %d (< "
                    "%d), restarting the scan (call"
                    " %u)", minLevel, lvl, calls);
#endif
            return minLevel;
        }
    }

    assert(q.empty());

#ifdef GNASH_DEBUG
    if (actionsToProcess) {
        log_debug(" Done processing actions in priority queue "
                "%d (call %u)", lvl, calls);
    }
#endif

    return minPopulatedPriorityQueue();
}

void
movie_root::flushHigherPriorityActionQueues()
{
    if( ! processingActions() )
    {
        // only flush the actions queue when we are 
        // processing the queue.
        // ie. we don't want to flush the queue 
        // during executing user event handlers,
        // which are not pushed at the moment.
        return;
    }

    if ( _disableScripts )
    {
        /// cleanup anything pushed later..
        clearActionQueue();
        return;
    }

    int lvl=minPopulatedPriorityQueue();
    while ( lvl<_processingActionLevel )
    {
        lvl = processActionQueue(lvl);
    }

}

void
movie_root::addLoadableObject(as_object* obj, std::auto_ptr<IOChannel> str)
{
    boost::shared_ptr<IOChannel> io(str.release());
    _loadCallbacks.push_back(LoadCallback(io, obj));
}

void
movie_root::addAdvanceCallback(ActiveRelay* obj)
{
    _objectCallbacks.insert(obj);
}

void
movie_root::removeAdvanceCallback(ActiveRelay* obj)
{
    _objectCallbacks.erase(obj);
}

void
movie_root::processActionQueue()
{
    if (_disableScripts) {
        /// cleanup anything pushed later..
        clearActionQueue();
        return;
    }

    _processingActionLevel = minPopulatedPriorityQueue();

    while (_processingActionLevel < PRIORITY_SIZE) {
        _processingActionLevel = processActionQueue(_processingActionLevel);
    }

    // Cleanup the stack.
    _vm.getStack().clear();

}

void
movie_root::pushAction(std::auto_ptr<ExecutableCode> code, size_t lvl)
{
    assert(lvl < PRIORITY_SIZE);
    _actionQueue[lvl].push_back(code.release());
}

void
movie_root::pushAction(const action_buffer& buf, DisplayObject* target)
{
#ifdef GNASH_DEBUG
    log_debug("Pushed action buffer for target %s", 
            target->getTargetPath());
#endif

    std::auto_ptr<ExecutableCode> code(new GlobalCode(buf, target));

    _actionQueue[PRIORITY_DOACTION].push_back(code.release());
}

void
movie_root::executeAdvanceCallbacks()
{

    if (!_objectCallbacks.empty()) {

        // Copy it, as the call can change the original, which is not only 
        // bad for invalidating iterators, but also allows infinite recursion.
        std::vector<ActiveRelay*> currentCallbacks;
        std::copy(_objectCallbacks.begin(), _objectCallbacks.end(),
                std::back_inserter(currentCallbacks));

        std::for_each(currentCallbacks.begin(), currentCallbacks.end(), 
                std::mem_fun(&ActiveRelay::update));
    }

    if (!_loadCallbacks.empty()) {
        _loadCallbacks.remove_if(
                std::mem_fun_ref(&movie_root::LoadCallback::processLoad));
    }

    processActionQueue();
}

void
movie_root::executeTimers()
{
#ifdef GNASH_DEBUG_TIMERS_EXPIRATION
        log_debug("Checking %d timers for expiry", _intervalTimers.size());
#endif

    unsigned long now = _vm.getTime();

    typedef std::multimap<unsigned int, Timer*> ExpiredTimers;
    ExpiredTimers expiredTimers;

    for (TimerMap::iterator it=_intervalTimers.begin(),
            itEnd=_intervalTimers.end(); it != itEnd; ) {

        // Get an iterator to next element, as we'll use
        // erase to drop cleared timers, and that would
        // invalidate the current iterator.
        //
        // FYI: it's been reported on ##iso-c++ that next
        //      C++ version will fix std::map<>::erase(iterator)
        //      to return the next valid iterator,
        //      like std::list<>::erase(iterator) does.
        //      For now, we'll have to handle this manually)
        //
        TimerMap::iterator nextIterator = it;
        ++nextIterator;

        Timer* timer = it->second;

        if (timer->cleared()) {
            // this timer was cleared, erase it
            delete timer;
            _intervalTimers.erase(it);
        }
        else {
            unsigned long elapsed;
            if (timer->expired(now, elapsed)) {
                expiredTimers.insert(std::make_pair(elapsed, timer));
            }
        }

        it = nextIterator;
    }

    foreachSecond(expiredTimers.begin(), expiredTimers.end(),
                  &Timer::executeAndReset);

    if (!expiredTimers.empty()) processActionQueue();

}

#ifdef GNASH_USE_GC
void
movie_root::markReachableResources() const
{
    foreachSecond(_movies.rbegin(), _movies.rend(), &MovieClip::setReachable);

    // Mark original top-level movie
    // This should always be in _movies, but better make sure
    if ( _rootMovie ) _rootMovie->setReachable();

    // Mark mouse entities 
    _mouseButtonState.markReachableResources();
    
    // Mark timer targets
    foreachSecond(_intervalTimers.begin(), _intervalTimers.end(),
                  &Timer::markReachableResources);

    std::for_each(_objectCallbacks.begin(), _objectCallbacks.end(),
            std::mem_fun(&ActiveRelay::setReachable));

    std::for_each(_loadCallbacks.begin(), _loadCallbacks.end(),
            std::mem_fun_ref(&movie_root::LoadCallback::setReachable));

    // Mark LoadMovieRequest handlers as reachable
    _movieLoader.setReachable();

    // Mark resources reachable by queued action code
    for (size_t lvl = 0; lvl < PRIORITY_SIZE; ++lvl)
    {
        const ActionQueue& q = _actionQueue[lvl];
        std::for_each(q.begin(), q.end(),
                std::mem_fun(&ExecutableCode::markReachableResources));
    }

    if (_currentFocus) _currentFocus->setReachable();

    // Mark DisplayObject being dragged, if any
    m_drag_state.markReachableResources();

    // NOTE: cleanupDisplayList() should have cleaned up all
    // unloaded live characters. The remaining ones should be marked
    // by their parents.
#if ( GNASH_PARANOIA_LEVEL > 1 ) || defined(ALLOW_GC_RUN_DURING_ACTIONS_EXECUTION)
    for (LiveChars::const_iterator i=_liveChars.begin(), e=_liveChars.end();
            i!=e; ++i) {
#ifdef ALLOW_GC_RUN_DURING_ACTIONS_EXECUTION
        (*i)->setReachable();
#else
        assert((*i)->isReachable());
#endif
    }
#endif
    
    // NOTE: cleanupUnloadedListeners() should have cleaned up all unloaded
    // key listeners. The remaining ones should be marked by their parents
#if ( GNASH_PARANOIA_LEVEL > 1 ) || defined(ALLOW_GC_RUN_DURING_ACTIONS_EXECUTION)
    for (LiveChars::const_iterator i=_keyListeners.begin(),
            e=_keyListeners.end(); i!=e; ++i) {
#ifdef ALLOW_GC_RUN_DURING_ACTIONS_EXECUTION
        (*i)->setReachable();
#else
        assert((*i)->isReachable());
#endif
    }
#endif

    // NOTE: cleanupUnloadedListeners() should have cleaned up all
    // unloaded mouse listeners. The remaining ones should be marked by
    // their parents
#if ( GNASH_PARANOIA_LEVEL > 1 ) || defined(ALLOW_GC_RUN_DURING_ACTIONS_EXECUTION)
    for (LiveChars::const_iterator i = _mouseListeners.begin(),
            e = _mouseListeners.end(); i!=e; ++i) {
#ifdef ALLOW_GC_RUN_DURING_ACTIONS_EXECUTION
        (*i)->setReachable();
#else
        assert((*i)->isReachable());
#endif
    }
#endif

}
#endif // GNASH_USE_GC

InteractiveObject*
movie_root::getTopmostMouseEntity(boost::int32_t x, boost::int32_t y) const
{

    for (Levels::const_reverse_iterator i=_movies.rbegin(), e=_movies.rend();
            i != e; ++i)
    {
        InteractiveObject* ret = i->second->topmostMouseEntity(x, y);
        if (ret) return ret;
    }

    return 0;
}

const DisplayObject *
movie_root::findDropTarget(boost::int32_t x, boost::int32_t y,
        DisplayObject* dragging) const
{

    for (Levels::const_reverse_iterator i=_movies.rbegin(), e=_movies.rend();
            i!=e; ++i) {
        
        const DisplayObject* ret = i->second->findDropTarget(x, y, dragging);
        if (ret) return ret;
    }
    return 0;
}

void
movie_root::cleanupDisplayList()
{

#define GNASH_DEBUG_INSTANCE_LIST 1

#ifdef GNASH_DEBUG_INSTANCE_LIST
    static size_t maxLiveChars = 0;
#endif

    // Let every sprite cleanup the local DisplayList
    //
    // TODO: we might skip this additional scan by delegating
    //       cleanup of the local DisplayLists in the ::display
    //       method of each sprite, but that will introduce 
    //       problems when we implement skipping ::display()
    //       when late on FPS. Alternatively we may have the
    //       MovieClip::markReachableResources take care
    //       of cleaning up unloaded... but that will likely
    //       introduce problems when allowing the GC to run
    //       at arbitrary times.
    //       The invariant to keep is that cleanup of unloaded DisplayObjects
    //       in local display lists must happen at the *end* of global action
    //       queue processing.
    foreachSecond(_movies.rbegin(), _movies.rend(),
                  &MovieClip::cleanupDisplayList);

    // Now remove from the instance list any unloaded DisplayObject
    // Note that some DisplayObjects may be unloaded but not yet destroyed,
    // in this case we'll also destroy them, which in turn might unload
    // further DisplayObjects, maybe already scanned, so we keep scanning
    // the list until no more unloaded-but-non-destroyed DisplayObjects
    // are found.
    // Keeping unloaded-but-non-destroyed DisplayObjects wouldn't really hurt
    // in that ::advanceLiveChars would skip any unloaded DisplayObjects.
    // Still, the more we remove the less work GC has to do...
    //

    bool needScan;
#ifdef GNASH_DEBUG_DLIST_CLEANUP
    int scansCount = 0;
#endif
    do {
#ifdef GNASH_DEBUG_DLIST_CLEANUP
        scansCount++;
        int cleaned =0;
#endif
        needScan=false;

        // Remove unloaded DisplayObjects from the _liveChars list
        for (LiveChars::iterator i=_liveChars.begin(), e=_liveChars.end(); i!=e;)
        {
            DisplayObject* ch = *i;
            if (ch->unloaded()) {
                // the sprite might have been destroyed already
                // by effect of an unload() call with no onUnload
                // handlers available either in self or child
                // DisplayObjects
                if (!ch->isDestroyed()) {

#ifdef GNASH_DEBUG_DLIST_CLEANUP
                    cout << ch->getTarget() << "(" << typeName(*ch) <<
                        ") was unloaded but not destroyed, destroying now" <<
                        endl;
#endif
                    ch->destroy();
                    // destroy() might mark already-scanned chars as unloaded
                    needScan = true; 
                }
#ifdef GNASH_DEBUG_DLIST_CLEANUP
                else {
                    cout << ch->getTarget() << "(" << typeName(*ch) <<
                        ") was unloaded and destroyed" << endl;
                }
#endif

                i = _liveChars.erase(i);

#ifdef GNASH_DEBUG_DLIST_CLEANUP
                cleaned++;
#endif
            }
            else ++i; 
        }

#ifdef GNASH_DEBUG_DLIST_CLEANUP
        cout << " Scan " << scansCount << " cleaned " << cleaned <<
            " instances" << endl;
#endif
    } while (needScan);

#ifdef GNASH_DEBUG_INSTANCE_LIST
    if ( _liveChars.size() > maxLiveChars )
    {
        maxLiveChars = _liveChars.size();
        log_debug("Global instance list grew to %d entries", maxLiveChars);
    }
#endif

}

void
movie_root::advanceLiveChar(DisplayObject* ch)
{
    if (!ch->unloaded())
    {
#ifdef GNASH_DEBUG
        log_debug("    advancing DisplayObject %s", ch->getTarget());
#endif
        ch->advance();
    }
#ifdef GNASH_DEBUG
    else {
        log_debug("    DisplayObject %s is unloaded, not advancing it",
                ch->getTarget());
    }
#endif
}

void
movie_root::advanceLiveChars()
{

#ifdef GNASH_DEBUG
    log_debug("---- movie_root::advance: %d live DisplayObjects in "
            "the global list", _liveChars.size());
#endif

    std::for_each(_liveChars.begin(), _liveChars.end(),
            boost::bind(advanceLiveChar, _1));
}

void
movie_root::set_background_color(const rgba& color)
{
    if (m_background_color_set) return;
    m_background_color_set = true;
    
    rgba newcolor = color;
    newcolor.m_a = m_background_color.m_a;

    if (m_background_color != color) {
        setInvalidated();
        m_background_color = color;
    }
}

void
movie_root::set_background_alpha(float alpha)
{

    boost::uint8_t newAlpha = clamp<int>(frnd(alpha * 255.0f), 0, 255);

    if (m_background_color.m_a != newAlpha) {
        setInvalidated();
        m_background_color.m_a = newAlpha;
    }
}

DisplayObject*
movie_root::findCharacterByTarget(const std::string& tgtstr) const
{
    if (tgtstr.empty()) return 0;

    string_table& st = _vm.getStringTable();

    // NOTE: getRootMovie() would be problematic in case the original
    //       root movie is replaced by a load to _level0... 
    //       (but I guess we'd also drop loadMovie requests in that
    //       case... just not tested)
    as_object* o = getObject(_movies.begin()->second);
    assert(o);

    std::string::size_type from = 0;
    while (std::string::size_type to = tgtstr.find('.', from))
    {
        std::string part(tgtstr, from, to - from);

        // TODO: there is surely a cleaner way to implement path finding.
        o = o->displayObject() ?
            o->displayObject()->pathElement(st.find(part)) :
            o->get_path_element(st.find(part));

        if (!o) {
#ifdef GNASH_DEBUG_TARGET_RESOLUTION
            log_debug("Evaluating DisplayObject target path: element "
                    "'%s' of path '%s' not found", part, tgtstr);
#endif
            return NULL;
        }
        if (to == std::string::npos) break;
        from = to + 1;
    }
    return get<DisplayObject>(o);
}

void
movie_root::getURL(const std::string& urlstr, const std::string& target,
        const std::string& data, MovieClip::VariablesMethod method)
{

    log_network("%s: HOSTFD is %d",  __FUNCTION__, _hostfd);
    
    if (_hostfd == -1) {
        /// If there is no hosting application, call the URL launcher. For
        /// safety, we resolve the URL against the base URL for this run.
        /// The data is not sent at all.
        URL url(urlstr, _runResources.baseURL());

        gnash::RcInitFile& rcfile = gnash::RcInitFile::getDefaultInstance();
        std::string command = rcfile.getURLOpenerFormat();

        /// Try to avoid letting flash movies execute
        /// arbitrary commands (sic)
        ///
        /// Maybe we should exec here, but if we do we might have problems
        /// with complex urlOpenerFormats like:
        ///    firefox -remote 'openurl(%u)'
        ///
        std::string safeurl = url.encode(urlstr);
        boost::replace_all(command, "%u", safeurl);
        
        log_debug (_("Launching URL: %s"), command);
        std::system(command.c_str());
        return;
    }

    /// This is when there is a hosting application.
    std::ostringstream request;
    std::string querystring;
    switch (method) {
      case MovieClip::METHOD_POST:
          request << "POST " << target << ":" << 
              data << "$" << urlstr << std::endl;
          break;
          
          // METHOD_GET and METHOD_NONE are the same, except that
          // for METHOD_GET we append the variables to the query
          // string.
      case MovieClip::METHOD_GET:
          // Append vars to URL query string
          if (urlstr.find("?") == std::string::npos) {
              querystring = "?";
          }
          else querystring = "&";
          querystring.append(data);
          
      case MovieClip::METHOD_NONE:
          // use the original url, non parsed (the browser will know
          // better how to resolve relative urls and handle
          // javascript)
          request << "GET " << target << ":" << urlstr << std::endl;
            break;
    }

    std::string requestString = request.str();
    size_t len = requestString.length();
    // TODO: should mutex-protect this ?
    // NOTE: we are assuming the hostfd is set in blocking mode here..
    log_debug(_("Attempt to write geturl requests fd %d"), _hostfd);

    int ret = write(_hostfd, requestString.c_str(), len);
    if (ret == -1) {
        log_error(_("Could not write to user-provided host requests "
                    "fd %d: %s"), _hostfd, std::strerror(errno));
    }
    if (static_cast<size_t>(ret) < len) {
        log_error(_("Could only write %d bytes over %d required to "
                    "user-provided host requests fd %d"),
                    ret, len, _hostfd);
    }

    // The request string ends with newline, and we don't want to log that
    requestString.resize(requestString.size() - 1);
    log_debug(_("Sent request '%s' to host fd %d"), requestString, _hostfd);

}

void
movie_root::setScriptLimits(boost::uint16_t recursion, boost::uint16_t timeout)
{

    // This tag reported in some sources to be ignored for movies
    // below SWF7. However, on Linux with PP version 9, the tag
    // takes effect on SWFs of any version.
    log_debug(_("Setting script limits: max recursion %d, "
            "timeout %d seconds"), recursion, timeout);

    _recursionLimit = recursion;
    _timeoutLimit = timeout;
    
}


#ifdef USE_SWFTREE
void
movie_root::getMovieInfo(tree<StringPair>& tr, tree<StringPair>::iterator it)
{

    tree<StringPair>::iterator localIter;

    //
    /// Stage
    //
    const movie_definition* def = _rootMovie->definition();
    assert(def);

    it = tr.insert(it, StringPair("Stage Properties", ""));

    localIter = tr.append_child(it, StringPair("Root VM version",
                def->isAS3() ? "AVM2 (unsupported)" : "AVM1"));
    
    std::ostringstream os;
    os << "SWF " << def->get_version();
    localIter = tr.append_child(it, StringPair("Root SWF version", os.str()));
    localIter = tr.append_child(it, StringPair("URL", def->get_url()));

    // TODO: format this better?
    localIter = tr.append_child(it, StringPair("Descriptive metadata",
                                        def->getDescriptiveMetadata()));
 
    /// Stage: real dimensions.
    os.str("");
    os << def->get_width_pixels() <<
        "x" << def->get_height_pixels();
    localIter = tr.append_child(it, StringPair("Real dimensions", os.str()));

    /// Stage: rendered dimensions.
    os.str("");
    os << m_viewport_width << "x" << m_viewport_height;
    localIter = tr.append_child(it, StringPair("Rendered dimensions", os.str()));

#if 0
    /// Stage: scaling allowed.
    localIter = tr.append_child(it, StringPair("Scaling allowed",
                _allowRescale ? yes : no));

    //  TODO: add _scaleMode, _valign and _haling info
#endif

    // Stage: scripts state (enabled/disabled)
    localIter = tr.append_child(it, StringPair("Scripts",
                _disableScripts ? " disabled" : "enabled"));
     
    getCharacterTree(tr, it);    
}

void
movie_root::getCharacterTree(tree<StringPair>& tr,
        tree<StringPair>::iterator it)
{

    tree<StringPair>::iterator localIter;

    /// Stage: number of live DisplayObjects
    std::ostringstream os;
    os << _liveChars.size();
    localIter = tr.append_child(it, StringPair(_("Live DisplayObjects"),
                os.str()));

    /// Live DisplayObjects tree
    for (LiveChars::const_iterator i=_liveChars.begin(), e=_liveChars.end();
                                                               i != e; ++i)
    {
        (*i)->getMovieInfo(tr, localIter);
    }

}

#endif

void
movie_root::handleFsCommand(const std::string& cmd, const std::string& arg)
    const
{
    if (_fsCommandHandler) _fsCommandHandler->notify(cmd, arg);
}

void
movie_root::errorInterface(const std::string& msg) const
{
    if (_interfaceHandler) _interfaceHandler->error(msg);
}

std::string
movie_root::callInterface(const std::string& cmd, const std::string& arg) const
{
    if (_interfaceHandler) return _interfaceHandler->call(cmd, arg);

    log_error("Hosting application registered no callback for events/queries");

    return "<no iface to hosting app>";
}

bool
isLevelTarget(int version, const std::string& name, unsigned int& levelno)
{
    if (version > 6) {
        if (name.compare(0, 6, "_level")) return false;
    }
    else {
        StringNoCaseEqual noCaseCmp;
        if (!noCaseCmp(name.substr(0, 6), "_level")) return false;
    }

    if (name.find_first_not_of("0123456789", 7) != std::string::npos) {
        return false;
    }
    // getting 0 here for "_level" is intentional
    levelno = std::strtoul(name.c_str() + 6, NULL, 0); 
    return true;

}

short
stringToStageAlign(const std::string& str)
{
    short am = 0;

    // Easy enough to do bitwise - std::bitset is not
    // really necessary!
    if (str.find_first_of("lL") != std::string::npos) {
        am |= 1 << movie_root::STAGE_ALIGN_L;
    } 

    if (str.find_first_of("tT") != std::string::npos) {
        am |= 1 << movie_root::STAGE_ALIGN_T;
    } 

    if (str.find_first_of("rR") != std::string::npos) {
        am |= 1 << movie_root::STAGE_ALIGN_R;
    } 

    if (str.find_first_of("bB") != std::string::npos) {
        am |= 1 << movie_root::STAGE_ALIGN_B;
    }

    return am;

}

void
movie_root::LoadCallback::setReachable() const
{
    _obj->setReachable();
}

bool
movie_root::LoadCallback::processLoad()
{

    if (!_stream) {
        callMethod(_obj, NSV::PROP_ON_DATA, as_value());
        return true;
    }

    const size_t chunksize = 65535;
    boost::uint8_t chunk[chunksize];

    size_t actuallyRead = _stream->readNonBlocking(chunk, chunksize);

    // We must still call onData if the stream is in error condition, e.g.
    // when an HTTP 404 error is returned.
    if (_stream->bad()) {
        callMethod(_obj, NSV::PROP_ON_DATA, as_value());
        return true;
    }

    if (actuallyRead) {

        // set total size only on first read
        if (_buf.empty()) {
            _obj->set_member(NSV::PROP_uBYTES_TOTAL, _stream->size());
        }

        _buf.append(chunk, actuallyRead);

        _obj->set_member(NSV::PROP_uBYTES_LOADED, _buf.size());

        log_debug("LoadableObject Loaded %d bytes, reaching %d/%d",
            actuallyRead, _buf.size(), _stream->size());
    }

    // We haven't finished till EOF 
    if (!_stream->eof()) return false;

    log_debug("LoadableObject reached EOF (%d/%d loaded)",
                _buf.size(), _stream->size());

    // got nothing, won't bother BOFs of nulls
    if (_buf.empty()) {
        callMethod(_obj, NSV::PROP_ON_DATA, as_value());
        return true;
    }

    // Terminate the string
    _buf.appendByte('\0');

    // Strip BOM, if any.
    // See http://savannah.gnu.org/bugs/?19915
    utf8::TextEncoding encoding;
    size_t size = _buf.size();

    // NOTE: the call below will possibly change 'size' parameter
    char* bufptr = utf8::stripBOM((char*)_buf.data(), size, encoding);
    if (encoding != utf8::encUTF8 && encoding != utf8::encUNSPECIFIED) {
        log_unimpl("%s to utf8 conversion in LoadableObject input parsing", 
                utf8::textEncodingName(encoding));
    }

    // NOTE: Data copy here !!
    as_value dataVal(bufptr); 

    // NOTE: we could release memory associated
    // with the buffer here, before invoking a new method,
    // but at the time of writing there's no method of SimpleBuffer
    // providing memory release except destruction. Will be
    // destroyed as soon as we return though...

    // NOTE: Another data copy here !
    callMethod(_obj, NSV::PROP_ON_DATA, dataVal);

    return true;
}

namespace {

// Return whether any action triggered by this event requires display redraw.
// See page about events_handling (in movie_interface.h)
//
/// TODO: make this code more readable !
bool
generate_mouse_button_events(movie_root& mr, MouseButtonState& ms)
{

    // Did this event trigger any action that needs redisplay ?
    bool need_redisplay = false;

    // TODO: have mouseEvent return
    // whether the action must trigger
    // a redraw.

    switch (ms.previousButtonState)
    {
        case MouseButtonState::DOWN:
        {
            // TODO: Handle trackAsMenu dragOver
            // Handle onDragOut, onDragOver
            if (!ms.wasInsideActiveEntity) {

                if (ms.topmostEntity == ms.activeEntity) {

                    // onDragOver
                    if (ms.activeEntity) {
                        ms.activeEntity->mouseEvent(event_id::DRAG_OVER);
                        need_redisplay=true;
                    }
                    ms.wasInsideActiveEntity = true;
                }
            }
            else if (ms.topmostEntity != ms.activeEntity) {
                // onDragOut
                if (ms.activeEntity) {
                    ms.activeEntity->mouseEvent(event_id::DRAG_OUT);
                    need_redisplay=true;
                }
                ms.wasInsideActiveEntity = false;
            }

            // Handle onRelease, onReleaseOutside
            if (ms.currentButtonState == MouseButtonState::UP) {
                // Mouse button just went up.
                ms.previousButtonState = MouseButtonState::UP;

                if (ms.activeEntity) {
                    if (ms.wasInsideActiveEntity) {
                        // onRelease
                        ms.activeEntity->mouseEvent(event_id::RELEASE);
                        need_redisplay = true;
                    }
                    else {
                        // TODO: Handle trackAsMenu 
                        // onReleaseOutside
                        ms.activeEntity->mouseEvent(event_id::RELEASE_OUTSIDE);
                        // We got out of active entity
                        ms.activeEntity = 0; // so we don't get RollOut next...
                        need_redisplay = true;
                    }
                }
            }
            return need_redisplay;
        }

        case MouseButtonState::UP:
        {
            // New active entity is whatever is below the mouse right now.
            if (ms.topmostEntity != ms.activeEntity)
            {
                // onRollOut
                if (ms.activeEntity) {
                    ms.activeEntity->mouseEvent(event_id::ROLL_OUT);
                    need_redisplay=true;
                }

                ms.activeEntity = ms.topmostEntity;

                // onRollOver
                if (ms.activeEntity) {
                    ms.activeEntity->mouseEvent(event_id::ROLL_OVER);
                    need_redisplay=true;
                }

                ms.wasInsideActiveEntity = true;
            }

            // mouse button press
            if (ms.currentButtonState == MouseButtonState::DOWN) {
                // onPress

                // Try setting focus on the new DisplayObject. This will handle
                // all necessary events and removal of current focus.
                // Do not set focus to NULL.
                if (ms.activeEntity) {
                    mr.setFocus(ms.activeEntity);

                    ms.activeEntity->mouseEvent(event_id::PRESS);
                    need_redisplay=true;
                }

                ms.wasInsideActiveEntity = true;
                ms.previousButtonState = MouseButtonState::DOWN;
            }
        }
        default:
              return need_redisplay;
    }

}

const DisplayObject*
getNearestObject(const DisplayObject* o)
{
    while (1) {
        assert(o);
        if (isReferenceable(*o)) return o;
        o = o->get_parent();
    }
}

as_object*
getBuiltinObject(movie_root& mr, string_table::key cl)
{
    Global_as& gl = *mr.getVM().getGlobal();

    as_value val;
    if (!gl.get_member(cl, &val)) return 0;
    return val.to_object(gl);
}


}


} // namespace gnash

