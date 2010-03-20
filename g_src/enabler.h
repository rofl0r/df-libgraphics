//some of this stuff is based on public domain code from nehe or opengl books over the years
//additions and modifications Copyright (c) 2008, Tarn Adams
//All rights reserved.  See game.cpp or license.txt for more information.

#ifndef ENABLER_H
#define ENABLER_H

#include "platform.h"
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#ifdef __APPLE__
# include <SDL_image/SDL_image.h>
#else
# include <SDL/SDL_image.h>
#endif

#include "GL/glew.h"

#include <map>
#include <vector>
#include <algorithm>
#include <utility>
#include <list>
#include <iostream>
#include <sstream>
#include <stack>
#include <queue>

using std::vector;
using std::pair;
using std::map;
using std::list;
using std::stack;
using std::queue;

#include "basics.h"
#include "svector.h"
#include "endian.h"
#include "files.h"
#include "enabler_input.h"

#define ENABLER

#ifndef BITS

#define BITS

#define BIT1 1
#define BIT2 2
#define BIT3 4
#define BIT4 8
#define BIT5 16
#define BIT6 32
#define BIT7 64
#define BIT8 128
#define BIT9 256
#define BIT10 512
#define BIT11 1024
#define BIT12 2048
#define BIT13 4096
#define BIT14 8192
#define BIT15 16384
#define BIT16 32768
#define BIT17 65536UL
#define BIT18 131072UL
#define BIT19 262144UL
#define BIT20 524288UL
#define BIT21 1048576UL
#define BIT22 2097152UL
#define BIT23 4194304UL
#define BIT24 8388608UL
#define BIT25 16777216UL
#define BIT26 33554432UL
#define BIT27 67108864UL
#define BIT28 134217728UL
#define BIT29 268435456UL
#define BIT30 536870912UL
#define BIT31 1073741824UL
#define BIT32 2147483648UL

#endif

#define GAME_TITLE_STRING "Dwarf Fortress"

char get_slot_and_addbit_uchar(unsigned char &addbit,long &slot,long checkflag,long slotnum);

class pstringst
{
 public:
  string dat;
};

class stringvectst
{
 public:
  svector<pstringst *> str;

  void add_string(const string &st)
  {
    pstringst *newp=new pstringst;
    newp->dat=st;
    str.push_back(newp);
  }

  long add_unique_string(const string &st)
  {
    long i;
    for(i=(long)str.size()-1;i>=0;i--)
      {
	if(str[i]->dat==st)return i;
      }
    add_string(st);
    return (long)str.size()-1;
  }

  void add_string(const char *st)
  {
    if(st!=NULL)
      {
	pstringst *newp=new pstringst;
	newp->dat=st;
	str.push_back(newp);
      }
  }

  void insert_string(long k,const string &st)
  {
    pstringst *newp=new pstringst;
    newp->dat=st;
    if(str.size()>k)str.insert(k,newp);
    else str.push_back(newp);
  }

  ~stringvectst()
    {
      clean();
    }

  void clean()
  {
    while(str.size()>0)
      {
	delete str[0];
	str.erase(0);
      }
  }

  void read_file(file_compressorst &filecomp,long loadversion)
  {
    long dummy;
    filecomp.read_file(dummy);
    str.resize(dummy);

    long s;
    for(s=0;s<dummy;s++)
      {
	str[s]=new pstringst;
	filecomp.read_file(str[s]->dat);
      }
  }
  void write_file(file_compressorst &filecomp)
  {
    long dummy=str.size();
    filecomp.write_file(dummy);

    long s;
    for(s=0;s<dummy;s++)
      {
	filecomp.write_file(str[s]->dat);
      }
  }
};

class flagarrayst
{
 public:
  flagarrayst()
    {
      slotnum=0;
      array=NULL;
    }
  ~flagarrayst()
    {
      if(array!=NULL)delete[] array;
      array=NULL;
      slotnum=0;
    }

  void set_size_on_flag_num(long flagnum)
  {
    if(flagnum<=0)return;

    set_size(((flagnum-1)>>3)+1);
  }

  void set_size(long newsize)
  {
    if(newsize<=0)return;

    if(array!=NULL)delete[] array;
    array=new unsigned char[newsize];
    memset(array,0,sizeof(unsigned char)*newsize);

    slotnum=newsize;
  }

  void clear_all()
  {
    if(slotnum<=0)return;

    if(array!=NULL)memset(array,0,sizeof(unsigned char)*slotnum);
  }

  void copy_from(flagarrayst &src)
  {
    clear_all();

    if(src.slotnum>0)
      {
	set_size(src.slotnum);
	memmove(array,src.array,sizeof(unsigned char)*slotnum);
      }
  }

  bool has_flag(long checkflag)
  {
    long slot;
    unsigned char addbit;
    if(get_slot_and_addbit_uchar(addbit,slot,checkflag,slotnum))
      {
	return (array[slot]&addbit)!=0;
      }
    else return false;
  }

  void add_flag(long checkflag)
  {
    long slot;
    unsigned char addbit;
    if(get_slot_and_addbit_uchar(addbit,slot,checkflag,slotnum))
      {
	array[slot]|=addbit;
      }
  }

  void toggle_flag(long checkflag)
  {
    long slot;
    unsigned char addbit;
    if(get_slot_and_addbit_uchar(addbit,slot,checkflag,slotnum))
      {
	array[slot]^=addbit;
      }
  }

  void remove_flag(long checkflag)
  {
    long slot;
    unsigned char addbit;
    if(get_slot_and_addbit_uchar(addbit,slot,checkflag,slotnum))
      {
	array[slot]&=~addbit;
      }
  }

  void write_file(file_compressorst &filecomp)
  {
    filecomp.write_file(slotnum);
    if(slotnum>0)
      {
	long ind;
	for(ind=0;ind<slotnum;ind++)filecomp.write_file(array[ind]);
      }
  }

  void read_file(file_compressorst &filecomp,long loadversion)
  {
    long newsl;
    filecomp.read_file(newsl);
    if(newsl>0)
      {
	//AVOID UNNECESSARY DELETE/NEW
	if(array!=NULL&&slotnum!=newsl)
	  {
	    delete[] array;
	    array=new unsigned char[newsl];
	  }
	if(array==NULL)array=new unsigned char[newsl];

	long ind;
	for(ind=0;ind<newsl;ind++)filecomp.read_file(array[ind]);

	slotnum=newsl;
      }
    else if(array!=NULL)
      {
	delete[] array;
	array=NULL;
      }
  }

 private:
  unsigned char *array;
  long slotnum;
};

#ifdef ENABLER

#define COLOR_BLACK 0
#define COLOR_BLUE 1
#define COLOR_GREEN 2
#define COLOR_CYAN 3
#define COLOR_RED 4
#define COLOR_MAGENTA 5
#define COLOR_YELLOW 6
#define COLOR_WHITE	7

enum ColorData
  {
    COLOR_DATA_WHITE_R,
    COLOR_DATA_WHITE_G,
    COLOR_DATA_WHITE_B,
    COLOR_DATA_RED_R,
    COLOR_DATA_RED_G,
    COLOR_DATA_RED_B,
    COLOR_DATA_GREEN_R,
    COLOR_DATA_GREEN_G,
    COLOR_DATA_GREEN_B,
    COLOR_DATA_BLUE_R,
    COLOR_DATA_BLUE_G,
    COLOR_DATA_BLUE_B,
    COLOR_DATA_YELLOW_R,
    COLOR_DATA_YELLOW_G,
    COLOR_DATA_YELLOW_B,
    COLOR_DATA_MAGENTA_R,
    COLOR_DATA_MAGENTA_G,
    COLOR_DATA_MAGENTA_B,
    COLOR_DATA_CYAN_R,
    COLOR_DATA_CYAN_G,
    COLOR_DATA_CYAN_B,
    COLOR_DATANUM
  };

#define TILEFLAG_DEAD BIT1
#define TILEFLAG_ROTATE BIT2
#define TILEFLAG_PIXRECT BIT3
#define TILEFLAG_HORFLIP BIT4
#define TILEFLAG_VERFLIP BIT5
#define TILEFLAG_LINE BIT6
#define TILEFLAG_RECT BIT7
#define TILEFLAG_BUFFER_DRAW BIT8
#define TILEFLAG_MODEL_PERSPECTIVE BIT9
#define TILEFLAG_MODEL_ORTHO BIT10
#define TILEFLAG_MODEL_TRANSLATE BIT11
#define TILEFLAG_LINE_3D BIT12

#define TRIMAX 9999

enum render_phase {
  setup, // 0
  complete,
  phase_count
};

class texture_bo {
  GLuint bo, tbo;
 public:
  texture_bo() { bo = tbo = 0; }
  void reset() {
    if (bo) {
      glDeleteBuffers(1, &bo);
      glDeleteTextures(1, &tbo);
      bo = tbo = 0;
      printGLError();
    }
  }
  void buffer(GLvoid *ptr, GLsizeiptr sz) {
    if (bo) reset();
    glGenBuffersARB(1, &bo);
    glGenTextures(1, &tbo);
    glBindBufferARB(GL_TEXTURE_BUFFER_ARB, bo);
    glBufferDataARB(GL_TEXTURE_BUFFER_ARB, sz, ptr, GL_STATIC_DRAW_ARB);
    printGLError();
  }
  void bind(GLenum texture_unit, GLenum type) {
    glActiveTexture(texture_unit);
    glBindTexture(GL_TEXTURE_BUFFER_ARB, tbo);
    glTexBufferARB(GL_TEXTURE_BUFFER_ARB, type, bo);
    printGLError();
  }
  GLuint texnum() { return tbo; }
};


class shader {
  string filename;
  std::ostringstream lines;
 public:
  std::ostringstream header;
  void load(const string &filename) {
    this->filename = filename;
    std::ifstream file(filename.c_str());
    string version;
    getline(file, version);
    header << version << std::endl;
    while (file.good()) {
      string line;
      getline(file, line);
      lines << line << std::endl;
    }
    file.close();
  }
  GLuint upload(GLenum type) {
    GLuint shader = glCreateShader(type);
    string lines_done = lines.str(), header_done = header.str();
    const char *ptrs[3];
    ptrs[0] = header_done.c_str();
    ptrs[1] = "#line 1 0\n";
    ptrs[2] = lines_done.c_str();
    glShaderSource(shader, 3, ptrs, NULL);
    glCompileShader(shader);
    // Let's see if this compiled correctly..
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) { // ..no. Check the compilation log.
      GLint log_size;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
      errorlog << filename << " preprocessed source:" << std::endl;
      std::cerr << filename << " preprocessed source:" << std::endl;
      errorlog << header_done << "#line 1 0\n" << lines_done;
      std::cerr << header_done << "#line 1 0\n" << lines_done;
      errorlog << filename << " shader compilation log (" << log_size << "):" << std::endl;
      std::cerr << filename << " shader compilation log (" << log_size << "):" << std::endl;
      char *buf = new char[log_size];
      glGetShaderInfoLog(shader, log_size, NULL, buf);
      errorlog << buf << std::endl;
      std::cerr << buf << std::endl;
      errorlog.flush();
      delete[] buf;
      MessageBox(NULL, "Shader compilation failed; details in errorlog.txt", "Critical error", MB_OK);
      abort();
    }
    printGLError();
    return shader;
  }
};


class text_info_elementst
{
 public:
  virtual string get_string()
  {
    string empty;
    return empty;
  }
  virtual long get_long()
  {
    return 0;
  }

  virtual ~text_info_elementst(){}
};

class text_info_element_stringst : public text_info_elementst
{
 public:
  virtual string get_string()
  {
    return str;
  }
  text_info_element_stringst(const string &newstr)
    {
      str=newstr;
    }

 protected:
  string str;
};

class text_info_element_longst : public text_info_elementst
{
 public:
  virtual long get_long()
  {
    return val;
  }
  text_info_element_longst(long nval)
    {
      val=nval;
    }

 protected:
  long val;
};

class text_infost
{
 public:
  svector<text_info_elementst *> element;

  void clean()
  {
    while(element.size()>0)
      {
	delete element[0];
	element.erase(0);
      }
  }

  string get_string(int e)
  {
    if(e<0||e>=element.size())
      {
	string empty;
	return empty;
      }
    if(element[e]==NULL)
      {
	string empty;
	return empty;
      }
    return element[e]->get_string();
  }

  long get_long(int e)
  {
    if(e<0||e>=element.size())
      {
	return 0;
      }
    if(element[e]==NULL)
      {
	return 0;
      }
    return element[e]->get_long();
  }

  ~text_infost()
    {
      clean();
    }
};

class text_system_file_infost
{
 public:
  long index;
  string filename;

  static text_system_file_infost *add_file_info(const string &newf,long newi,char newft)
  {
    return new text_system_file_infost(newf,newi,newft);
  }

  void initialize_info();
  void get_text(text_infost &text);
  void get_specific_text(text_infost &text,long num);

 protected:
  char file_token;
  long number;

  text_system_file_infost(const string &newf,long newi,char newft)
    {
      filename=newf;
      file_token=newft;
      index=newi;
      number=0;
    }
};

class text_systemst
{
 public:
  void register_file_fixed(const string &file_name,long index,char token,char initialize)
  {
    text_system_file_infost *tsfi=text_system_file_infost::add_file_info(file_name,index,token);
    if(initialize)tsfi->initialize_info();
    file_info.push_back(tsfi);
  }
  void register_file(const string &file_name,long &index,char token,char initialize)
  {
    long t;
    for(t=(long)file_info.size()-1;t>=0;t--)
      {
	if(file_info[t]->filename==file_name)
	  {
	    //RESET CALLING INDEX AND BAIL IF THIS FILE IS ALREADY IN THE SYSTEM
	    index=file_info[t]->index;
	    return;
	  }
      }

    text_system_file_infost *tsfi=text_system_file_infost::add_file_info(file_name,index,token);
    if(initialize)tsfi->initialize_info();
    file_info.push_back(tsfi);
  }
  void initialize_system()
  {
    long t;
    for(t=(long)file_info.size()-1;t>=0;t--)file_info[t]->initialize_info();
  }
  void get_text(long index,text_infost &text)
  {
    long t;
    for(t=(long)file_info.size()-1;t>=0;t--)
      {
	if(file_info[t]->index==index)
	  {
	    file_info[t]->get_text(text);
	    return;
	  }
      }
  }
  void get_text(const string &file_name,text_infost &text)
  {
    long t;
    for(t=(long)file_info.size()-1;t>=0;t--)
      {
	if(file_info[t]->filename==file_name)
	  {
	    file_info[t]->get_text(text);
	    return;
	  }
      }
  }
  void get_specific_text(long index,text_infost &text,long num)
  {
    long t;
    for(t=(long)file_info.size()-1;t>=0;t--)
      {
	if(file_info[t]->index==index)
	  {
	    file_info[t]->get_specific_text(text,num);
	    return;
	  }
      }
  }

  ~text_systemst()
    {
      while(file_info.size()>0)
	{
	  delete file_info[0];
	  file_info.erase(0);
	}
    }

 protected:
  svector<text_system_file_infost *> file_info;
};

class curses_text_boxst
{
	public:
		stringvectst text;

		void add_paragraph(stringvectst &src,long para_width);
		void add_paragraph(const string &src,long para_width);

		void read_file(file_compressorst &filecomp,long loadversion)
			{
			text.read_file(filecomp,loadversion);
			}
		void write_file(file_compressorst &filecomp)
			{
			text.write_file(filecomp);
			}
		void clean()
			{
			text.clean();
			}
};

#define COPYTEXTUREFLAG_HORFLIP BIT1
#define COPYTEXTUREFLAG_VERFLIP BIT2

#define ENABLERFLAG_RENDER BIT1
#define ENABLERFLAG_MAXFPS BIT2

// GL texture positions
struct gl_texpos {
  GLfloat left, right, top, bottom;
};

// Being a texture catalog interface, with opengl and sdl capability
class textures
{
  friend class enablerst;
  friend class renderer_opengl;
 private:
  vector<SDL_Surface *> raws;
  bool uploaded;
  long add_texture(SDL_Surface*);
 protected:
  GLuint gl_catalog; // texture catalog gennum
  struct gl_texpos *gl_texpos; // Texture positions in the GL catalog, if any
 public:
  // Initialize state variables
  textures() {
    uploaded=false;
    gl_texpos = NULL;
  }
  int textureCount() {
    return raws.size();
  }
  // Upload in-memory textures to the GPU, or delete them
  // When textures are uploaded, any alteration to a texture
  // is automatically reflected in the uploaded copy - eg. it's replaced.
  // This is very expensive in opengl mode. Don't do it often.
  void upload_textures();
  // Also, you really should make sure to remove uploaded textures before
  // deleting a window.
  void remove_uploaded_textures();
  // Returns the most recent texture data
  SDL_Surface *get_texture_data(long pos);
  // Clone a texture
  long clone_texture(long src);
  // Remove all color, but not transparency
  void grayscale_texture(long pos);
  // Loads dimx*dimy textures from a file, assuming all tiles
  // are equally large and arranged in a grid
  // Texture positions are saved in row-major order to tex_pos
  // If convert_magenta is true and the file does not have built-in transparency,
  // any magenta (255,0,255 RGB) is converted to full transparency
  // The calculated size of individual tiles is saved to disp_x, disp_y
  void load_multi_pdim(const string &filename,long *tex_pos,long dimx,long dimy,
		       bool convert_magenta,
/* 		       long *adj_x, long *adj_y, */
		       long *disp_x, long *disp_y);
  // Loads a single texture from a file, returning the handle
  long load(const string &filename, bool convert_magenta);
/*   SDL_Surface *get_texture_data(long pos); */
/*   SDL_Surface *load_bitmap_file(const string &filename); */
/*   SDL_Surface *load_bitmap_file_with_alpha(const string &filename); */
  // To delete a texture..
  void delete_texture(long pos);
};

struct tile {
  int x, y;
  long tex;
};

typedef struct {									// Window Creation Info
  char*				title;						// Window Title
  int					width;						// Width
  int					height;						// Height
  int					bitsPerPixel;				// Bits Per Pixel
  BOOL				isFullScreen;				// FullScreen?
} GL_WindowInit;									// GL_WindowInit

typedef struct {									// Contains Information Vital To A Window
  GL_WindowInit		init;						// Window Init
  BOOL				isVisible;				// Window Visible?
} GL_Window;								// GL_Window

enum zoom_commands { zoom_in, zoom_out, zoom_reset };


struct texture_fullid {
  int texpos;
  float r, g, b;
  float br, bg, bb;

  bool operator< (const struct texture_fullid &other) const {
    if (texpos != other.texpos) return texpos < other.texpos;
    if (r != other.r) return r < other.r;
    if (g != other.g) return g < other.g;
    if (b != other.b) return b < other.b;
    if (br != other.br) return br < other.br;
    if (bg != other.bg) return bg < other.bg;
    return bb < other.bb;
  }
};

class renderer {
  unsigned char *screen;
  long *screentexpos;
  char *screentexpos_addcolor;
  unsigned char *screentexpos_grayscale;
  unsigned char *screentexpos_cf;
  unsigned char *screentexpos_cbr;
  // For partial printing:
  unsigned char *screen_old;
  long *screentexpos_old;
  char *screentexpos_addcolor_old;
  unsigned char *screentexpos_grayscale_old;
  unsigned char *screentexpos_cf_old;
  unsigned char *screentexpos_cbr_old;
  // For async rendering (so NULL otherwise)
  unsigned char *screen_spare;
  long *screentexpos_spare;
  char *screentexpos_addcolor_spare;
  unsigned char *screentexpos_grayscale_spare;
  unsigned char *screentexpos_cf_spare;
  unsigned char *screentexpos_cbr_spare;

 protected:
  void gps_allocate(int x, int y);
  texture_fullid screen_to_texid(int x, int y);
 public:
  void display();
  virtual void update_tile(int x, int y) = 0;
  virtual void update_all() = 0;
  virtual void render() = 0;
  virtual void set_fullscreen() {} // Should read from enabler.is_fullscreen()
  virtual void zoom(zoom_commands cmd) {};
  virtual void resize(int w, int h) = 0;
  void swap_buffers();
  renderer() {
    screen = NULL;
    screentexpos = NULL;
    screentexpos_addcolor = NULL;
    screentexpos_grayscale = NULL;
    screentexpos_cf = NULL;
    screentexpos_cbr = NULL;
    screen_old = NULL;
    screentexpos_old = NULL;
    screentexpos_addcolor_old = NULL;
    screentexpos_grayscale_old = NULL;
    screentexpos_cf_old = NULL;
    screentexpos_cbr_old = NULL;
    screen_spare = NULL;
    screentexpos_spare = NULL;
    screentexpos_addcolor_spare = NULL;
    screentexpos_grayscale_spare = NULL;
    screentexpos_cf_spare = NULL;
    screentexpos_cbr_spare = NULL;
  }
};

class enablerst : public enabler_inputst
{
  friend class initst;
  friend class renderer_2d;
  friend class renderer_opengl;

  string command_line;
  bool fullscreen;
  stack<pair<int,int> > overridden_grid_sizes;

  class renderer *renderer;
  void eventLoop_SDL();
#ifdef CURSES
  void eventLoop_ncurses();
#endif
  
  // Frame timing calculations
  int calculated_fps, calculated_gfps;
  queue<int> frame_timings, gframe_timings; // Milisecond lengths of the last few frames
  int frame_sum, gframe_sum;
  int frame_last, gframe_last; // SDL_GetTick returns
  void do_update_fps(queue<int> &q, int &sum, int &last, int &calc);
  void update_fps();
  void update_gfps();
  SDL_cond *timer_cond; // Triggered once per gframe, for wake-up calls
  SDL_mutex *dummy_mutex;
  int fps, gfps;

  // Barriers for async rendering
  SDL_sem *run_frame, *done_frame;
  int outstanding_frames; // How many async loops are outstanding
  void trigger_async_loop() {
    if (outstanding_frames > fps) {
      // Too many outstanding frames
      SDL_SemWait(done_frame);
    } else
      outstanding_frames++;
    SDL_SemPost(run_frame);
  }
  void quiesce_async_loop() {
    // Attempt to cancel frames first
    while (SDL_SemTryWait(run_frame) == 0)
      outstanding_frames--;
    // Then wait for remaining ones (should just be 1) to finish
    while (outstanding_frames) {
      SDL_SemWait(done_frame);
      update_fps();
      outstanding_frames--;
    }
  }
  void reap_async_loop() {
    // Check for finished frames
    while (SDL_SemTryWait(done_frame) == 0) {
      outstanding_frames--;
      update_fps();
    }
  }
  
 public:

  float ccolor[16][3]; // The curses-RGB mapping used for non-curses display modes
  
  enablerst();
  unsigned long flag; // ENABLERFLAG_RENDER, ENABLERFLAG_MAXFPS

  int loop(string cmdline);
  void async_loop();
  void do_frame();

  // Renderer interface
  // The update functions read their data from gps, and do whatever preparations are necessary
  // for render() to do its job
  void update_tile(int x, int y) { renderer->update_tile(x,y); }
  void update_all() { renderer->update_all(); } // Renders the *entire* array, thus reducing function calls
  void render() { renderer->render(); }         // Draw the frame to screen
  
  // Framerate interface
  void set_fps(int fps);
  void set_gfps(int gfps);
  int get_fps() { return fps; }
  int get_gfps() { return gfps; }
  int calculate_fps();  // Calculate the actual provided (G)FPS
  int calculate_gfps();

  // Mouse interface, such as it is
  void get_mouse_coords(int &x, int &y);
  char mouse_lbut,mouse_rbut,mouse_lbut_down,mouse_rbut_down,mouse_lbut_lift,mouse_rbut_lift;

  // OpenGL state (wrappers)
  class textures textures; // Font/graphics texture catalog
  GLsync sync; // Rendering barrier
  
  // Grid-size interface
  void override_grid_size(int w, int h); // Pick a /particular/ grid-size
  void release_grid_size(); // Undoes override_grid_size
  void zoom_display(zoom_commands command);

  
  // Window management
  bool is_fullscreen() { return fullscreen; }
  void toggle_fullscreen() {
    fullscreen = !fullscreen;
    renderer->set_fullscreen();
  }
  
/*   friend class gridrectst; */
/*   friend class tilest; */
/*   friend class textures; */
  
/*   unsigned long flag; // ENABLERFLAG_RENDER, ENABLERFLAG_MAXFPS */
/*   char tracking_on;   // Whether we're tracking the mouse or not */

/*   string command_line; */


/*   int loop(void); */

/*   void terminate_application(GL_Window* window); */
/*   char create_full_screen; */
/*   char inactive_mode; */
/*   void reshape_GL(int width,int height); */
/*   void render(enum render_phase); */
/*   void render_tiles(enum render_phase, bool clear); */
/*   void graphicsinit(); */
/*   long gridrect_create(long dimx,long dimy); */
/*   long cursesrect_create(long font_id,long x,long y,long dimx,long dimy); */
/*   gridrectst *get_gridrect(long rect_id); */
/*   void toggle_fullscreen() */
/*   { */
/*     toggle_fullscreen(&window); */
/*   } */
/*   // Display a particular tile at a particular window-relative location */
/*   // Pass -1 as tex_pos to remove a particular tile. Using the same id twice */
/*   // overwrites the last call. */
/*   void set_tile(long tex_pos, int id, int x, int y); */
/*   void add_gennum_tile(long gennum,double dimx,double dimy,double letx,double lety,char alpha); */
/*   void set_color(float r,float g,float b,float a=1); */
/*   void enable_fade(float r,float g,float b,float a,float t); */
/*   void disable_fade(); */
/*   void set_font(long font_id) */
/*   { */
/*     active_font_id=font_id; */
/*   } */
/*   void print_string(const string &str,char centered=0,short length_lim=1000,char crammed_lets=0); */
/*   void set_center(double newx,double newy) */
/*   { */
/*     center_x=newx; */
/*     center_y=newy; */
/*   } */
/*   void set_clear_color(float newr,float newg,float newb,float newa) */
/*   { */
/*     if(fade_t==0) */
/*       { */
/* 	clear_r=newr;clear_g=newg;clear_b=newb;clear_a=newa; */
/*       } */
/*     else */
/*       { */
/* 	clear_r=newr*(1.0f-fade_t)+fade_r*fade_t; */
/* 	clear_g=newg*(1.0f-fade_t)+fade_g*fade_t; */
/* 	clear_b=newb*(1.0f-fade_t)+fade_b*fade_t; */
/* 	clear_a=newa*(1.0f-fade_t)+fade_a*fade_t; */
/*       } */
/*   } */
/*   unsigned long get_timer() */
/*   { */
/*     if(main_qprate.QuadPart==0)return 0; */
/*     else return qpc.QuadPart; */
/*   } */

/* void copy_texture_data(unsigned char *dest,long destx,long desty,char destalpha, */
/* 			unsigned char *src,long srcx,long srcy,char srcalpha, */
/* 			long offx,long offy, */
/* 			float rmult,float gmult,float bmult,char use_trans,long *color_data=NULL,unsigned long flag=0); */

/* void save_texture_data_to_bmp(unsigned char *bitmapImage,long dimx,long dimy,long alpha,string &filename); */


/*  void flip_uchar_array(unsigned char *buff,long dimx,long dimy,long bytes_per_pixel,unsigned long flag); */
/*  void get_texture_data_dims(long pos,short &width,short &height,char &alpha); */

/*   void read_pixels(int x,int y,int width,int height,unsigned char *buffer); */

/*   enablerst(); */
/*   ~enablerst() */
/*     { */
/*       long t; */
/*       for(t=(long)gridrect.size()-1;t>=0;t--) */
/* 	{ */
/* 	  delete gridrect[t]; */
/* 	} */
/*       gridrect.clear(); */
/*     } */
/*   void enable_buffer_draw(){buffer_draw=1;} */
/*   void disable_buffer_draw(){buffer_draw=0;} */
/*   char doing_buffer_draw(){return buffer_draw;} */

/*   LARGE_INTEGER qpfr; */
/*   LARGE_INTEGER main_qprate,qprate,g_qprate; */
/*   LARGE_INTEGER qpc,qpc2,g_qpc; */
		
/*   double frames_outstanding; */

/*   Uint32 now; //we only need to be getting the tick time once */

/*   unsigned long current_render_count; */
/*   unsigned long secondary_render_count; */

/*   long oldmouse_x,oldmouse_y,mouse_x,mouse_y; */
/*   char mouse_lbut_down,mouse_rbut_down; */
/*   char mouse_lbut_lift,mouse_rbut_lift; */

/*   text_systemst text_system; */
/*   char change_screen_resolution (int width, int height, int bitsPerPixel); */

/*   void refresh_tiles(); */

/*   int window_width,window_height; */

/*   void do_frame(); */
/*   class textures textures; */

/*  private: */
/*   long next_tile_slot; */
/*   long active_font_id; */

/*   char is_program_looping; */
/*   svector<gridrectst *> gridrect; */
/*   long next_gridrect_id; */
/*   long next_texture_id; */
/*   long next_texture_data_id; */
/*   double locx,locy,locz; */
/*   double center_x,center_y; */
/*   float color_r,color_g,color_b,color_a; */
/*   float clear_r,clear_g,clear_b,clear_a; */
/*   float fade_r,fade_g,fade_b,fade_a,fade_t; */
/*   char buffer_draw; */

/*   // Tile-map used by set_tile */
/*   map<int,struct tile> tiles; */
/*   // Vertex and texture arrays generated from above map */
/*   GLfloat *tile_vertices; */
/*   GLfloat *tile_texcoords; */


/*   char create_window_GL (GL_Window* window); */
/*   char destroy_window_GL (GL_Window* window); */
/*   char register_window_class (void); */
/*   void toggle_fullscreen(GL_Window* window); */
/*   void create_textures(); */
/*   void remove_textures(); */

/*   GLsync sync; */

/*  public: */
/*   bool RunningMacro; */
/*   bool use_opengl; // If false, everything goes via SDL 2D output instead */

/*   void reset_gl(GL_Window* window); */
/*   void reset_gl() */
/*   { */
/*     reset_gl(&window); */
/*   } */

/*   bool prep_for_image_export(); */
/*   void post_image_export(); */
  
};

void convert_to_rgb(float &r,float &g,float &b,char col,char bright);
#endif

/* pair<int,int> window_to_grid(int x, int y); */
/* pair<int,int> resize_grid(const int w_req, const int h_req); */

// Function prototypes for deep-DF calls
char beginroutine();
char mainloop();
void endroutine();

extern enablerst enabler;

#endif //ENABLER_H
