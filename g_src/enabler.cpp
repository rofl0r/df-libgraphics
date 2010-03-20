#ifdef __APPLE__
# include "osx_messagebox.h"
#elif defined(unix)
# include <gtk/gtk.h>
#endif

#include <cassert>

#include "platform.h"
#include "enabler.h"
#include "init.h"
#include "music_and_sound_g.h"

using namespace std;

enablerst enabler;


// For the printGLError macro
int glerrorcount = 0;

// Set to 0 when the game wants to quit
static int loopvar = 1;

// Reports an error to the user, using a MessageBox and stderr.
static void report_error(const char *error_preface, const char *error_message)
{
  char *buf = NULL;
  // +4 = +colon +space +newline +nul
  buf = new char[strlen(error_preface) + strlen(error_message) + 4];
  sprintf(buf, "%s: %s\n", error_preface, error_message);
  MessageBox(NULL, buf, "Error", MB_OK);
  fprintf(stderr, "%s", buf);
  delete [] buf;
}

texture_fullid renderer::screen_to_texid(int x, int y) {
  struct texture_fullid ret;
  const int tile = x * gps.dimy + y;
  const int ch = screen[tile*4 + 0];
  const int bold = (screen[tile*4 + 3] != 0) * 8;
  const int fg = screen[tile*4 + 1] + bold;
  const int bg = screen[tile*4 + 2] + bold;

  assert(fg >= 0 && fg < 16);
  assert(bg >= 0 && bg < 16);
  
  ret.texpos = enabler.is_fullscreen() ?
    init.font.large_font_texpos[ch] :
    init.font.small_font_texpos[ch];
  ret.r = enabler.ccolor[fg][0];
  ret.g = enabler.ccolor[fg][1];
  ret.b = enabler.ccolor[fg][2];
  ret.br = enabler.ccolor[bg][0];
  ret.bg = enabler.ccolor[bg][1];
  ret.bb = enabler.ccolor[bg][2];

  // TODO: Account for graphics mode

  return ret;
}


#ifdef CURSES
# include "renderer_curses.cpp"
#endif
#include "renderer_2d.cpp"
#include "renderer_opengl.cpp"

// The frame timer probably doesn't absolutely need to use globals,
// but this code *works*. I'm not touching it.
#ifdef unix
// sig_atomic_t is guaranteed to be atomic.
static volatile sig_atomic_t gframes_outstanding;
#else
// int *should* be, but is not guaranteed. OTOH, on windows it is.
// Also, if the very rare race condition does somehow trigger, and mess
// things up (which it shouldn't), the next check will clamp it anyway.
static volatile signed int gframes_outstanding;
#endif
static double frames_outstanding;

static Uint32 timer_tick(Uint32 interval, void *param) {
  SDL_cond *c = (SDL_cond*)param;
  gframes_outstanding++;
  const int fps = enabler.get_fps(),
    gfps = enabler.get_gfps();
  double fps_per_gfps = (double)fps / (double)gfps;

  frames_outstanding += fps_per_gfps;
  SDL_CondSignal(c); // Wake up the sleeper if necessary
  return interval;
}

enablerst::enablerst() {
  fullscreen = false;
  sync = NULL;
  renderer = NULL;
  timer_cond = NULL;
  dummy_mutex = NULL;
  calculated_fps = calculated_gfps = frame_sum = gframe_sum = frame_last = gframe_last = 0;
  fps = 100; gfps = 20;
  run_frame = SDL_CreateSemaphore(0);
  done_frame = SDL_CreateSemaphore(0);
}

void renderer::display()
{
  const int dimx = init.display.grid_x;
  const int dimy = init.display.grid_y;
  static bool use_graphics = init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS);
  if (gps.force_full_display_count) {
    memcpy(screen_old, screen, dimx*dimy*4*sizeof *screen);
    if (use_graphics) {
      memcpy(screentexpos_old, screentexpos, dimx*dimy*sizeof *screentexpos);
      memcpy(screentexpos_addcolor_old, screentexpos_addcolor, dimx*dimy*sizeof *screentexpos_addcolor);
      memcpy(screentexpos_grayscale_old, screentexpos_grayscale, dimx*dimy*sizeof *screentexpos_grayscale);
      memcpy(screentexpos_cf_old, screentexpos_cf, dimx*dimy*sizeof *screentexpos_cf);
      memcpy(screentexpos_cbr_old, screentexpos_cbr, dimx*dimy*sizeof *screentexpos_cbr);
    }
    // Update the entire screen
    enabler.update_all();
  } else {
    if (use_graphics) {
      for (int x2=0; x2 < dimx; x2++) {
        for (int y2=0; y2 < dimy; y2++) {
          const int off = x2*dimy*4 + y2*4;
          // Partial printing (and color-conversion): Big-ass if.
          if (*(int*)(screen + off) == *(int*)(screen_old + off) &&
              screentexpos[off] == screentexpos_old[off] &&
              screentexpos_addcolor[off] == screentexpos_addcolor_old[off] &&
              screentexpos_grayscale[off] == screentexpos_grayscale_old[off] &&
              screentexpos_cf[off] == screentexpos_cf_old[off] &&
              screentexpos_cbr[off] == screentexpos_cbr_old[off])
            {
              // Nothing's changed, this clause deliberately empty
            } else {
            // Okay, the buffer has changed. First update the old-buffer.
            *(int*)(screen_old + off) = *(int*)(screen + off);
            screentexpos_old[off] = screentexpos[off];
            screentexpos_addcolor_old[off] = screentexpos_addcolor[off];
            screentexpos_grayscale_old[off] = screentexpos_grayscale[off];
            screentexpos_cf_old[off] = screentexpos_cf[off];
            screentexpos_cbr_old[off] = screentexpos_cbr[off];
            
            update_tile(x2, y2);
          }
        }
      }
    } else {
      for (int x2=0; x2 < dimx; x2++) {
        for (int y2=0; y2 < dimy; y2++) {
          const int off = x2*dimy*4 + y2*4;
          // sizeof(int) = 4.. I very much hope.
          if (*(int*)(screen + off) != *(int*)(screen_old + off)) {
            *(int*)(screen_old + off) = *(int*)(screen + off);
            update_tile(x2, y2);
          }
        }
      }
    }
  }
  if (gps.force_full_display_count > 0) gps.force_full_display_count--;
}

void renderer::gps_allocate(int x, int y) {
  if (screen) delete[] screen;
  if (screentexpos) delete[] screentexpos;
  if (screentexpos_addcolor) delete[] screentexpos_addcolor;
  if (screentexpos_grayscale) delete[] screentexpos_grayscale;
  if (screentexpos_cf) delete[] screentexpos_cf;
  if (screentexpos_cbr) delete[] screentexpos_cbr;
  if (screen_old) delete[] screen_old;
  if (screentexpos_old) delete[] screentexpos_old;
  if (screentexpos_addcolor_old) delete[] screentexpos_addcolor_old;
  if (screentexpos_grayscale_old) delete[] screentexpos_grayscale_old;
  if (screentexpos_cf_old) delete[] screentexpos_cf_old;
  if (screentexpos_cbr_old) delete[] screentexpos_cbr_old;
  if (screen_spare) delete[] screen_spare;
  if (screentexpos_spare) delete[] screentexpos_spare;
  if (screentexpos_addcolor_spare) delete[] screentexpos_addcolor_spare;
  if (screentexpos_grayscale_spare) delete[] screentexpos_grayscale_spare;
  if (screentexpos_cf_spare) delete[] screentexpos_cf_spare;
  if (screentexpos_cbr_spare) delete[] screentexpos_cbr_spare;
  
  gps.screen = screen = new unsigned char[x*y*4];
  memset(screen, 0, x*y*4);
  gps.screentexpos = screentexpos = new long[x*y];
  memset(screentexpos, 0, x*y*sizeof(long));
  gps.screentexpos_addcolor = screentexpos_addcolor = new char[x*y];
  memset(screentexpos_addcolor, 0, x*y);
  gps.screentexpos_grayscale = screentexpos_grayscale = new unsigned char[x*y];
  memset(screentexpos_grayscale, 0, x*y);
  gps.screentexpos_cf = screentexpos_cf = new unsigned char[x*y];
  memset(screentexpos_cf, 0, x*y);
  gps.screentexpos_cbr = screentexpos_cbr = new unsigned char[x*y];
  memset(screentexpos_cbr, 0, x*y);

  screen_old = new unsigned char[x*y*4];
  memset(screen_old, 0, x*y*4);
  screentexpos_old = new long[x*y];
  memset(screentexpos_old, 0, x*y*sizeof(long));
  screentexpos_addcolor_old = new char[x*y];
  memset(screentexpos_addcolor_old, 0, x*y);
  screentexpos_grayscale_old = new unsigned char[x*y];
  memset(screentexpos_grayscale_old, 0, x*y);
  screentexpos_cf_old = new unsigned char[x*y];
  memset(screentexpos_cf_old, 0, x*y);
  screentexpos_cbr_old = new unsigned char[x*y];
  memset(screentexpos_cbr_old, 0, x*y);


  // In async mode, gps operations draw into the spare arrays, while
  // the renderer draws the normal set
  if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_ASYNC)) {
    gps.screen = screen_spare = new unsigned char[x*y*4];
    memset(screen_spare, 0, x*y*4);
    gps.screentexpos = screentexpos_spare = new long[x*y];
    memset(screentexpos_spare, 0, x*y*sizeof(long));
    gps.screentexpos_addcolor = screentexpos_addcolor_spare = new char[x*y];
    memset(screentexpos_addcolor_spare, 0, x*y);
    gps.screentexpos_grayscale = screentexpos_grayscale_spare = new unsigned char[x*y];
    memset(screentexpos_grayscale_spare, 0, x*y);
    gps.screentexpos_cf = screentexpos_cf_spare = new unsigned char[x*y];
    memset(screentexpos_cf_spare, 0, x*y);
    gps.screentexpos_cbr = screentexpos_cbr_spare = new unsigned char[x*y];
    memset(screentexpos_cbr_spare, 0, x*y);
  }
  
  gps.resize(x,y);
}

void renderer::swap_buffers() {
  // Swap spare and main arrays
  screen_spare = screen; screen = gps.screen; gps.screen = screen_spare;
  screentexpos_spare = screentexpos; screentexpos = gps.screentexpos; gps.screentexpos = screentexpos_spare;
  screentexpos_addcolor_spare = screentexpos_addcolor; screentexpos_addcolor = gps.screentexpos_addcolor; gps.screentexpos_addcolor = screentexpos_addcolor_spare;
  screentexpos_grayscale_spare = screentexpos_grayscale; screentexpos_grayscale = gps.screentexpos_grayscale; gps.screentexpos_grayscale = screentexpos_grayscale_spare;
  screentexpos_cf_spare = screentexpos_cf; screentexpos_cf = gps.screentexpos_cf; gps.screentexpos_cf = screentexpos_cf_spare;
  screentexpos_cbr_spare = screentexpos_cbr; screentexpos_cbr = gps.screentexpos_cbr; gps.screentexpos_cbr = screentexpos_cbr_spare;
}

void enablerst::async_loop() {
  for (;;) {
    // Wait until we're supposed to run a frame
    SDL_SemWait(run_frame);
    int ret = mainloop();
    if (ret) loopvar = 0;
    // Notify main thread of frame completion
    SDL_SemPost(done_frame);
    // And quit if done
    if (ret) return;
  }
}

void enablerst::do_frame() {
  // printf("f: %g, g: %d\n", frames_outstanding, gframes_outstanding);
  const double fps_per_gfps = (double)fps / (double)gfps;
  // Clamp max number of outstanding frames/gframes to a sane number
  // Yes, this is a race condition, but the comparison and assignment are
  // individually atomic so worst-case it's clamped to 5 when it "should" be 6
  // or something. No problem there.
  if (gframes_outstanding > 5) gframes_outstanding = 5;
  if (frames_outstanding > fps_per_gfps*2+1) frames_outstanding = fps_per_gfps*2+1;
  //ENABLERFLAG_MAXFPS can lead to negative values, at least for frames_outstanding
  if (gframes_outstanding < -5) gframes_outstanding = -5;
  if (frames_outstanding < -20) frames_outstanding = -20;

  static const bool render_async = init.display.flag.has_flag(INIT_DISPLAY_FLAG_ASYNC);

  // If we're in async mode, we use last frame's buffers and last frame's render flag
  unsigned long cached_flag = flag;
  
  // Run the main loop, if appropriate.
  if (frames_outstanding >= 1 || (flag & ENABLERFLAG_MAXFPS)) {
    // puts("loop");
    frames_outstanding -= 1;
    if (render_async) {
      if ((flag & ENABLERFLAG_RENDER) && gframes_outstanding) {
        render_things(); // Render the UI
        flag &= ~ENABLERFLAG_RENDER;
        // Swap display buffers
        renderer->swap_buffers();
      }
      // And run mainloop()
      trigger_async_loop();
    } else {
      if (mainloop())
        loopvar = 0;
      update_fps();
      cached_flag = flag; // Since we're not in async mode, we use this frame's flag
    }
  }
  
  // Render one graphical frame, if appropriate.
  // TODO: Move sync to renderer_opengl
  if ((cached_flag & ENABLERFLAG_RENDER) && gframes_outstanding > 0 &&
      (!sync || glClientWaitSync(sync, 0, 0) == GL_ALREADY_SIGNALED)) {
    if (sync) {
      glDeleteSync(sync);
      sync = NULL;
    }
    if (!render_async)
      render_things(); // Call UI renderers in DF proper - but not while mainloop is running!
    // Render
    renderer->display();
    renderer->render();
    if (!render_async)
      flag &= ~ENABLERFLAG_RENDER; // Mark this rendering as complete.
    update_gfps();
  } else {
    gframes_outstanding = 0;
  }

  if (render_async)
    reap_async_loop();
  
  // if (flag & ENABLERFLAG_MAXFPS)
  //   puts("max");
  // if (frames_outstanding > 0)
  //   std::cout << "frames: " << frames_outstanding << '\n';
  
  // Sleep if appropriate
  if (gframes_outstanding <= 0 &&
      frames_outstanding <= 0) {
    // Sleep until the timer thread signals us
    if(!(flag & ENABLERFLAG_MAXFPS))SDL_CondWait(timer_cond, dummy_mutex);
  }
}

void enablerst::eventLoop_SDL()
{
  
  SDL_Event event;
  const SDL_Surface *screen = SDL_GetVideoSurface();
  Uint32 mouse_lastused = 0;
  SDL_ShowCursor(SDL_DISABLE);
  static const bool render_async = init.display.flag.has_flag(INIT_DISPLAY_FLAG_ASYNC);
 
  // Initialize the grid
  renderer->resize(screen->w, screen->h);

  while (loopvar) {
    Uint32 now = SDL_GetTicks();

    while (SDL_PollEvent(&event)) {
      // Make sure mainloop isn't running while we're processing input
      if (render_async) quiesce_async_loop();
      // Handle SDL events
      switch (event.type) {
      case SDL_KEYDOWN:
        // Disable mouse if it's been long enough
        if (mouse_lastused + 5000 < now) {
          if(init.input.flag.has_flag(INIT_INPUT_FLAG_MOUSE_PICTURE)) {
            // hide the mouse picture
            // enabler.set_tile(0, TEXTURE_MOUSE, enabler.mouse_x, enabler.mouse_y);
          }
          SDL_ShowCursor(SDL_DISABLE);
        }
      case SDL_KEYUP:
      case SDL_QUIT:
        enabler.add_input(event, now);
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        if (!init.input.flag.has_flag(INIT_INPUT_FLAG_MOUSE_OFF)) {
          int isdown = (event.type == SDL_MOUSEBUTTONDOWN);
          if (event.button.button == SDL_BUTTON_LEFT) {
            enabler.mouse_lbut = isdown;
            enabler.mouse_lbut_down = isdown;
            if (!isdown)
              enabler.mouse_lbut_lift = 0;
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            enabler.mouse_rbut = isdown;
            enabler.mouse_rbut_down = isdown;
            if (!isdown)
              enabler.mouse_rbut_lift = 0;
          } else
            enabler.add_input(event, now);
        }
        break;
      case SDL_ACTIVEEVENT:
        enabler.clear_input();
        if (event.active.state & SDL_APPACTIVE) {
          if (event.active.gain) {
            enabler.flag|=ENABLERFLAG_RENDER;
            gps.force_full_display_count++;
          }
        }
        break;
      case SDL_VIDEOEXPOSE:
        gps.force_full_display_count++;
        enabler.flag|=ENABLERFLAG_RENDER;
        break;
      case SDL_MOUSEMOTION:
        // Deal with the mouse hiding bit
        mouse_lastused = now;
        if(init.input.flag.has_flag(INIT_INPUT_FLAG_MOUSE_PICTURE)) {
          // turn on mouse picture
          // enabler.set_tile(gps.tex_pos[TEXTURE_MOUSE], TEXTURE_MOUSE,enabler.mouse_x, enabler.mouse_y);
        } else {
          SDL_ShowCursor(SDL_ENABLE);
        }
        // Is the mouse over the screen surface?
        // if(!init.input.flag.has_flag(INIT_INPUT_FLAG_MOUSE_OFF)) {
        //   if (event.motion.x >= origin_x && event.motion.x < origin_x + size_x &&
        //       event.motion.y >= origin_y && event.motion.y < origin_y + size_y) {
        //     // Store last position
        //     enabler.oldmouse_x = enabler.mouse_x;
        //     enabler.oldmouse_y = enabler.mouse_y;
        //     enabler.tracking_on = 1;
        //     // Set viewport_x/y as appropriate, and fixup mouse position for zoom
        //     // We use only the central 60% of the window for setting viewport origin.
        //     if (!zoom_grid) {
        //       double mouse_x = (double)event.motion.x / size_x,
        //         mouse_y = (double)event.motion.y / size_y;
        //       double percentage = 0.60;
        //       mouse_x /= percentage;
        //       mouse_y /= percentage;
        //       mouse_x -= (1-percentage)/2;
        //       mouse_y -= (1-percentage)/2;
        //       mouse_x = MIN(MAX(mouse_x,0),1);
        //       mouse_y = MIN(MAX(mouse_y,0),1);
        //       double new_viewport_x = mouse_x, new_viewport_y = mouse_y;
        //       if (new_viewport_x != viewport_x || new_viewport_y != viewport_y) {
        //         viewport_x = new_viewport_x;
        //         viewport_y = new_viewport_y;
        //         gps.force_full_display_count++;
        //       }
        //       double visible = 1/viewport_zoom,
        //         invisible = 1 - visible;
        //       double visible_w = enabler.window_width * visible,
        //         visible_h = enabler.window_height * visible;
        //       enabler.mouse_x = ((double)event.motion.x / enabler.window_width) * visible_w + (invisible*viewport_x*enabler.window_width);
        //       enabler.mouse_y = ((double)event.motion.y / enabler.window_height) * visible_h + (invisible*viewport_y*enabler.window_height);
        //     } else {
        //       enabler.mouse_x = event.motion.x;
        //       enabler.mouse_y = event.motion.y;
        //     }
        //   } else {
        //     enabler.oldmouse_x = -1;
        //     enabler.oldmouse_y = -1;
        //     enabler.mouse_x = -1;
        //     enabler.mouse_y = -1;
        //     enabler.mouse_lbut = 0;
        //     enabler.mouse_rbut = 0;
        //     enabler.mouse_lbut_lift = 0;
        //     enabler.mouse_rbut_lift = 0;
        //     enabler.tracking_on = 0;
        //   }
        // } //init mouse on
        break;
      case SDL_VIDEORESIZE:
        if (is_fullscreen())
          errorlog << "Caught resize event in fullscreen??\n";
        else {
          gamelog << "Resizing window to " << event.resize.w << "x" << event.resize.h << endl << flush;
          renderer->resize(event.resize.w, event.resize.h);
        }
        break;
      } // switch (event.type)
    } //while have event
  
    do_frame();
#if !defined(NO_FMOD)
    // Call FMOD::System.update(). Manages a bunch of sound stuff.
    musicsound.update();
#endif
  }
}

int enablerst::loop(string cmdline) {
  command_line = cmdline;

  // Call DF's initialization routine
  if (!beginroutine())
    exit(EXIT_FAILURE);

  // Initialize an interval timer for gframes, with
  // a semaphore that the main loop can sleep on.
  SDL_cond *c = SDL_CreateCond();
  SDL_TimerID timer = SDL_AddTimer(1000 / gfps, timer_tick, c);
  timer_cond = c;
  // We need this to keep SDL happy
  dummy_mutex = SDL_CreateMutex();
  SDL_mutexP(dummy_mutex);
  frames_outstanding = 0;
  
  // Allocate a renderer
  if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_TEXT)) {
#ifdef CURSES
    renderer = new renderer_curses();
#else
    report_error("PRINT_MODE","TEXT not supported on windows");
    exit(EXIT_FAILURE);
#endif
  } else if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_2D)) {
    renderer = new renderer_2d();
  } else if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_ACCUM_BUFFER)) {
    renderer = new renderer_accum_buffer();
  } else if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_FRAME_BUFFER)) {
    renderer = new renderer_framebuffer();
  } else if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT)) {
    if (init.display.partial_print_count)
      renderer = new renderer_partial();
    else
      renderer = new renderer_once();
  } else if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_VBO)) {
    renderer = new renderer_vbo();
  } else {
    renderer = new renderer_opengl();
  }

  // At this point we should have a window that is setup to render DF.
  if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_TEXT)) {
#ifdef CURSES
    eventLoop_ncurses();
#endif
  } else {
    SDL_EnableUNICODE(1);
    eventLoop_SDL();
  }

  endroutine();

  // Clean up graphical resources
  delete renderer;
}

void enablerst::override_grid_size(int x, int y) {
  overridden_grid_sizes.push(make_pair<int,int>(init.display.grid_x,init.display.grid_y));
  init.display.grid_x = x;
  init.display.grid_y = y;
}

void enablerst::release_grid_size() {
  if (!overridden_grid_sizes.size()) return;
  pair<int,int> sz = overridden_grid_sizes.top();
  overridden_grid_sizes.pop();

  init.display.grid_x = sz.first;
  init.display.grid_y = sz.second;
}

void enablerst::zoom_display(zoom_commands command) {
  renderer->zoom(command);
}

int enablerst::calculate_fps() { return calculated_fps; }
int enablerst::calculate_gfps() { return calculated_gfps; }

void enablerst::do_update_fps(queue<int> &q, int &sum, int &last, int &calc) {
  while (q.size() && sum > 10000) {
    sum -= q.front();
    q.pop();
  }
  const int now = SDL_GetTicks();
  const int interval = now - last;
  q.push(interval);
  sum += interval;
  last = now;
  if (sum)
    calc = q.size() * 1000 / sum;
}

void enablerst::update_fps() {
  do_update_fps(frame_timings, frame_sum, frame_last, calculated_fps);
}

void enablerst::update_gfps() {
  do_update_fps(gframe_timings, gframe_sum, gframe_last, calculated_gfps);
}

void enablerst::set_fps(int fps) {
  if (fps == 0)
    fps = 1048576;
  this->fps = fps;
}

void enablerst::set_gfps(int gfps) {
  this->gfps = gfps;
}

int call_loop(void *dummy) {
  enabler.async_loop();
  return 0;
}

int main (int argc, char* argv[]) {
#if !defined(__APPLE__) && defined(unix)
  bool ok = gtk_init_check(&argc, &argv);
  if (!ok) {
    puts("Display initialization failed, DF will crash if asked to use a messagebox.");
  }
#endif

  init.begin(); // Load init.txt settings
#if !defined(__APPLE__) && defined(unix)
  if (!ok && !init.display.flag.has_flag(INIT_DISPLAY_FLAG_TEXT)) {
    puts("Display not found and PRINT_MODE not set to TEXT, aborting.");
    exit(EXIT_FAILURE);
  }
#endif
  
#ifdef linux
  if (!init.media.flag.has_flag(INIT_MEDIA_FLAG_SOUND_OFF)) {
    // Initialize OpenAL
    if (!musicsound.initsound()) {
      puts("Initializing OpenAL failed, no sound will be played");
      init.media.flag.add_flag(INIT_MEDIA_FLAG_SOUND_OFF);
    }
  }
#endif

  // Initialise relevant SDL subsystems.
  int retval = SDL_Init(SDL_INIT_TIMER | (init.display.flag.has_flag(INIT_DISPLAY_FLAG_TEXT) ? 0 : SDL_INIT_VIDEO));
  // Report failure?
  if (retval != 0) {
    report_error("SDL initialization failure", SDL_GetError());
    return false;
  }

  // Load keyboard map
  keybinding_init();
  enabler.load_keybindings("data/init/interface.txt");

  string cmdLine;
  for (int i = 1; i < argc; ++i) { 
    char *option = argv[i];
    cmdLine += option;
    cmdLine += " ";
  }
  // Spawn computation thread, if we're in async mode
  if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_ASYNC)) {
    SDL_CreateThread(call_loop, NULL);
  }
  int result = enabler.loop(cmdLine);

  SDL_Quit();

#ifdef CURSES
  if (init.display.flag.has_flag(INIT_DISPLAY_FLAG_TEXT))
    endwin();
#endif
  
  return result;
}

char get_slot_and_addbit_uchar(unsigned char &addbit,long &slot,long checkflag,long slotnum)
{
  if(checkflag<0)return 0;

  //FIND PROPER SLOT
  slot=checkflag>>3;
  if(slot>=slotnum)return 0;

  //FIND PROPER BIT IN THAT SLOT
  addbit=1<<(checkflag%8);

  return 1;
}
