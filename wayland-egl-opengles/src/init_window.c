// gcc -o test init_window.c -I. -lwayland-client -lwayland-server -lwayland-client-protocol -lwayland-egl -lEGL -lGLESv2

#include <webgl/webgl1.h>

#include <wayland-client-core.h>
#include <wayland-client.h>
//#include <wayland-server.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h> // Wayland EGL MUST be included before EGL headers

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1.h"

#include "init_window.h"
#include "log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include <sys/time.h>

#include <GLES2/gl2.h>

#include <assert.h>

struct wl_compositor *compositor = NULL;
struct wl_surface *surface;
struct wl_egl_window *egl_window;
//struct wl_region *region;
struct  zxdg_decoration_manager_v1 *deco_manager;
struct zxdg_toplevel_decoration_v1 *deco;

struct xdg_wm_base *XDGWMBase;
struct xdg_surface *XDGSurface;
struct xdg_toplevel *XDGToplevel;

struct _escontext ESContext = {
  .native_display = 0/*NULL*/,
	.window_width = 0,
	.window_height = 0,
	.native_window  = 0,
	.display = NULL,
	.context = NULL,
	.surface = NULL
};

#define TRUE 1
#define FALSE 0

#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500

bool program_alive;
int32_t old_w, old_h;

GLuint program;
GLuint rotation_uniform;
GLuint pos;
GLuint col;
GLuint texture_id;  // Texture handle.
GLint sampler;      // Sampler location.
GLuint vertexBuffer[3];   // Vertex Buffer.
GLuint mvpLoc; // Uniform location.
//ESMatrix mvpMatrix;

unsigned viewportWidth_;
unsigned viewportHeight_;

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h,
		struct wl_array *states) {

	// no window geometry event, ignore
	if(w == 0 && h == 0) return;

	// window resized
	if(old_w != w && old_h != h) {
		old_w = w;
		old_h = h;

		wl_egl_window_resize((struct wl_egl_window *)ESContext.native_window, w, h, 0, 0);
		wl_surface_commit(surface);
	}
}

static void xdg_toplevel_handle_close(void *data,
		struct xdg_toplevel *xdg_toplevel) {
	// window closed, be sure that this event gets processed
	program_alive = false;
}

struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};


static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
		uint32_t serial) {
	// confirm that you exist to the compositor
	xdg_surface_ack_configure(xdg_surface, serial);

}

const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
		uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};


static void setup_deco(void)
{
	if (deco_manager) {
		deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
				deco_manager, XDGToplevel);

		zxdg_toplevel_decoration_v1_set_mode(deco,
				ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
}
void CreateNativeWindow(char *title, int width, int height) {
	old_w = WINDOW_WIDTH;
	old_h = WINDOW_HEIGHT;

	/*region = wl_compositor_create_region(compositor);

	wl_region_add(region, 0, 0, width, height);
	wl_surface_set_opaque_region(surface, region);*/

	struct wl_egl_window *egl_window = 
		wl_egl_window_create(surface, width, height);

	if (egl_window == EGL_NO_SURFACE) {
		LOG("No window !?\n");
		exit(1);
	}
	else LOG("Window created !\n");
	ESContext.window_width = width;
	ESContext.window_height = height;
	ESContext.native_window = (int)egl_window;

}

EGLBoolean CreateEGLContext ()
{
	EGLint numConfigs;
	EGLint majorVersion;
	EGLint minorVersion;
	EGLContext context;
	EGLSurface surface;
	EGLConfig config;
	EGLint fbAttribs[] =
	{
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE,        8,
		EGL_GREEN_SIZE,      8,
		EGL_BLUE_SIZE,       8,
		EGL_NONE
	};
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
	EGLDisplay display = eglGetDisplay( ESContext.native_display );
	if ( display == EGL_NO_DISPLAY )
	{
		LOG("No EGL Display...\n");
		return EGL_FALSE;
	}

	// Initialize EGL
	if ( !eglInitialize(display, &majorVersion, &minorVersion) )
	{
		LOG("No Initialisation...\n");
		return EGL_FALSE;
	}

	// Get configs
	if ( (eglGetConfigs(display, NULL, 0, &numConfigs) != EGL_TRUE) || (numConfigs == 0))
	{
		LOG("No configuration...\n");
		return EGL_FALSE;
	}

	// Choose config
	if ( (eglChooseConfig(display, fbAttribs, &config, 1, &numConfigs) != EGL_TRUE) || (numConfigs != 1))
	{
		LOG("No configuration...\n");
		return EGL_FALSE;
	}

	// Create a surface
	surface = eglCreateWindowSurface(display, config, ESContext.native_window, NULL);
	if ( surface == EGL_NO_SURFACE )
	{
		LOG("No surface...\n");
		return EGL_FALSE;
	}

	LOG("Surface=%d\n", surface);

	// Create a GL context
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs );
	if ( context == EGL_NO_CONTEXT )
	{
		LOG("No context...\n");
		return EGL_FALSE;
	}

	// Make the context current
	if ( !eglMakeCurrent(display, surface, surface, context) )
	{
		LOG("Could not make the current window current !\n");
		return EGL_FALSE;
	}

	ESContext.display = display;
	ESContext.surface = surface;
	ESContext.context = context;
	return EGL_TRUE;
}

EGLBoolean CreateWindowWithEGLContext(char *title, int width, int height) {
	CreateNativeWindow(title, width, height);
	return CreateEGLContext();
}

void draw() {
  /*glClearColor(0.5, 0.3, 0.2, 1.0);

	struct timeval tv;

	gettimeofday(&tv, NULL);

	float time = tv.tv_sec + tv.tv_usec/1000000.0;

	static GLfloat vertex_data[] = {
		0.6, 0.6, 1.0,
		-0.6, -0.6, 1.0,
		0.0, 1.0, 1.0
	};

	for(int i=0; i<3; i++) {
		vertex_data[i*3+0] = vertex_data[i*3+0]*cos(time) - vertex_data[i*3+1]*sin(time); 
		vertex_data[i*3+1] = vertex_data[i*3+0]*sin(time) + vertex_data[i*3+1]*cos(time); 
	}

	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertex_data);
	glEnableVertexAttribArray(0);

	glDrawArrays(GL_TRIANGLES, 0, 3);*/

  /*glViewport(0, 0, ESContext.window_width, ESContext.window_width);

  // Clear the color buffer.
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  static const GLfloat verts[] = { 0.0f, 0.5f, 0.0f,
                                   -0.5, -0.5f, 0.0f,
                                   0.5f, -0.5f, 0.0f };
  
  glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
  glEnableVertexAttribArray(pos);

  glDrawArrays(GL_TRIANGLES, 0, 3);

  glDisableVertexAttribArray(pos);*/

  static const GLfloat verts[] = { -0.5, -0.5, 0.0f,
				   0.5, -0.5, 0.0f,
				   0, 0.5, 0.0f };
  
  static const GLfloat colors[] = { 1, 0, 0,
				    0, 1, 0,
				    0, 0, 1};
  
  GLfloat angle;
  GLfloat rotation[4][4] = {
      {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
  
  static const int32_t speed_div = 2;
  static unsigned long start_time = 0;

  struct timeval tv;

  gettimeofday(&tv, NULL);

  /*unsigned long time = (unsigned long)(tv.tv_sec*1000 + tv.tv_usec/1000.0);

  if (start_time == 0)
    start_time = time;
  
    unsigned long cur_time = time;*/

  ++start_time;

  //printf("draw: %lu\n", start_time);

  glViewport(0, 0, ESContext.window_width, ESContext.window_width);

  // Clear the color buffer.
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  
  angle = ((start_time) / speed_div) % 360 * M_PI / 180.0;
  
  rotation[0][0] = cos(angle);
  rotation[0][2] = sin(angle);
  rotation[2][0] = -sin(angle);
  rotation[2][2] = cos(angle);

  glUniformMatrix4fv(rotation_uniform, 1, GL_FALSE,
                     (GLfloat*)rotation);
  
  glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
  glEnableVertexAttribArray(pos);

  glVertexAttribPointer(col, 3, GL_FLOAT, GL_FALSE, 0, colors);
  glEnableVertexAttribArray(col);

  glDrawArrays(GL_TRIANGLES, 0, 3);

  glDisableVertexAttribArray(pos);
  glDisableVertexAttribArray(col);
}

unsigned long last_click = 0;
void RefreshWindow() { eglSwapBuffers(ESContext.display, ESContext.surface); }

static void global_registry_handler
(void *data, struct wl_registry *registry, uint32_t id,
 const char *interface, uint32_t version) {
	LOG("Got a registry event for %s id %d\n", interface, id);
	if (strcmp(interface, "wl_compositor") == 0)
		compositor = 
			wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	else if(strcmp(interface, xdg_wm_base_interface.name) == 0) {
		XDGWMBase = wl_registry_bind(registry, id,
				&xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(XDGWMBase, &xdg_wm_base_listener, NULL);
	}
	else if (strcmp(interface, "zxdg_decoration_manager_v1") == 0) {
	  deco_manager = wl_registry_bind(registry, id,
			&zxdg_decoration_manager_v1_interface, 1);
	}
}

static void global_registry_remover
(void *data, struct wl_registry *registry, uint32_t id) {
	LOG("Got a registry losing event for %d\n", id);
}

const struct wl_registry_listener listener = {
	global_registry_handler,
	global_registry_remover
};

static void
get_server_references() {

	struct wl_display * display = wl_display_connect(NULL);
	if (display == NULL) {
		LOG("Can't connect to wayland display !?\n");
		exit(1);
	}
	LOG("Got a display !");

	struct wl_registry *wl_registry =
		wl_display_get_registry(display);
	wl_registry_add_listener(wl_registry, &listener, NULL);

	// This call the attached listener global_registry_handler
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	// If at this point, global_registry_handler didn't set the 
	// compositor, nor the shell, bailout !
	if (compositor == NULL || XDGWMBase == NULL) {
		LOG("No compositor !? No XDG !! There's NOTHING in here !\n");
		exit(1);
	}
	else {
		LOG("Okay, we got a compositor and a shell... That's something !\n");
		ESContext.native_display = (int)display;
	}
}

void destroy_window() {
	eglDestroySurface(ESContext.display, ESContext.surface);
	wl_egl_window_destroy((struct wl_egl_window *)ESContext.native_window);
	xdg_toplevel_destroy(XDGToplevel);
	xdg_surface_destroy(XDGSurface);
	wl_surface_destroy(surface);
	eglDestroyContext(ESContext.display, ESContext.context);
}

/*const char* vert_shader_text =
    "attribute vec4 pos;\n"
    "void main() {\n"
    "  gl_Position = pos;\n"
    "}\n";

const char* frag_shader_text =
    "precision mediump float;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
    "}\n";*/

const char* vert_shader_text =
    "uniform mat4 rotation;\n"
    "attribute vec4 pos;\n"
    "attribute vec4 color;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_Position = rotation * pos;\n"
    "  v_color = color;\n"
    "}\n";

const char* frag_shader_text =
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_FragColor = v_color;\n"
    "}\n";

static GLuint create_shader(const char* source, GLenum shader_type) {
  GLuint shader;
  GLint status;

  shader = glCreateShader(shader_type);
  assert(shader != 0);

  glShaderSource(shader, 1, (const char**)&source, NULL);
  glCompileShader(shader);

  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    char log[1000];
    GLsizei len;
    glGetShaderInfoLog(shader, 1000, &len, log);
    fprintf(stderr, "Error: compiling %s: %*s\n",
            shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment", len, log);
    exit(1);
  }

  return shader;
}

void init_gl(unsigned width, unsigned height,
    const char* vertShaderText, const char* fragShaderText) {
  GLuint frag, vert;
  GLint status;

  viewportWidth_ = width;
  viewportHeight_ = height;

  frag = create_shader(fragShaderText, GL_FRAGMENT_SHADER);
  vert = create_shader(vertShaderText, GL_VERTEX_SHADER);

  program = glCreateProgram();
  glAttachShader(program, frag);
  glAttachShader(program, vert);
  glLinkProgram(program);

  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (!status) {
    char log[1000];
    GLsizei len;
    glGetProgramInfoLog(program, 1000, &len, log);
    fprintf(stderr, "Error: linking:\n%*s\n", len, log);
    exit(1);
  }

  glUseProgram(program);

  pos = 0;
  col = 1;

  // VertexBufferObject IDs
  vertexBuffer[0] = 0;
  vertexBuffer[1] = 0;
  vertexBuffer[2] = 0;

  glBindAttribLocation(program, pos, "pos");
  glBindAttribLocation(program, col, "color");
  glLinkProgram(program);

  // Return the location of a uniform variable.
  rotation_uniform = glGetUniformLocation(program, "rotation");
}


int main() {
	get_server_references();

	surface = wl_compositor_create_surface(compositor);
	if (surface == NULL) {
		LOG("No Compositor surface ! Yay....\n");
		exit(1);
	}
	else LOG("Got a compositor surface !\n");

	XDGSurface = xdg_wm_base_get_xdg_surface(XDGWMBase, surface);

	xdg_surface_add_listener(XDGSurface, &xdg_surface_listener, NULL);

	XDGToplevel = xdg_surface_get_toplevel(XDGSurface);
	xdg_toplevel_set_title(XDGToplevel, "Wayland EGL example");
	xdg_toplevel_add_listener(XDGToplevel, &xdg_toplevel_listener, NULL);

	setup_deco();

	wl_surface_commit(surface);

	CreateWindowWithEGLContext("Nya", WINDOW_WIDTH, WINDOW_HEIGHT);

	init_gl(WINDOW_WIDTH, WINDOW_HEIGHT, vert_shader_text, frag_shader_text);

	int i;
	
	emscripten_glGetIntegerv(0x821B, &i);

	LOG("OpenGL version major = %d\n", i);
	  
	program_alive = true;

	while (program_alive) {
	  wl_display_dispatch_pending((struct wl_display *)ESContext.native_display);
		draw();
		RefreshWindow();

		usleep(1000);
	}

	destroy_window();
	wl_display_disconnect((struct wl_display *)ESContext.native_display);
	LOG("Display disconnected !\n");

	exit(0);
}
