#include "sp_plus.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "linux_audio.c"

struct x_window_data {
	Display *display;
	Window window;
	XImage *x_window_buffer;
	GC default_gc;
	XVisualInfo visinfo; 

	int width;			// width of screen in pixels
	int height;			// height of screen in pixels
	int pixel_bytes;		// bytes per pixel
	int pixel_buf_size;		// width * height * pixel_bytes
	char *pixel_buf;		// pixels to be rendered to window
};


// initializes a window and populates data struct
// data struct will be initialized inside init_x_window
void init_x_window(struct x_window_data *data)
{
	// TODO store these somewhere better
	// create display connection
	const int width = 800;
	const int height = 600;

	Display *display = XOpenDisplay(NULL);
	if (!display) {
		fprintf(stderr, "Could not open display\n");
		exit(1);
	}

	int root = XDefaultRootWindow(display);
	int default_screen = XDefaultScreen(display);

	// get hardware visual info
	const int screen_bit_depth = 24;
	XVisualInfo visinfo = {0};
	if (!XMatchVisualInfo(
				display, 
				default_screen, 
				screen_bit_depth, 
				TrueColor, 
				&visinfo)) {
		fprintf(stderr, "No matching visual info\n");
		exit(1);
	}

	// set some attributes
	XSetWindowAttributes attributes = {0};
	attributes.background_pixel = 0;
	attributes.colormap = 
		XCreateColormap(display, root, visinfo.visual, AllocNone);
	attributes.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask;
	attributes.bit_gravity = StaticGravity;	// prevents flickering on window resize
	unsigned long attribute_mask = 
		CWBackPixel | CWColormap | CWEventMask | CWBitGravity;

	// create window
	Window window = XCreateWindow(
			display, root, 0, 0, 
			width, height, 0, visinfo.depth, 
			InputOutput, visinfo.visual, 
			attribute_mask, &attributes);
	if (!window) {
		fprintf(stderr, "Could not create window\n");
		exit(1);
	}

	// window manager attrributes
	XStoreName(display, window, "SP-PLUS");

	// TODO store these somewhere useful
	// set minimum window size
	const int min_width = 400;
	const int min_height = 300;
	XSizeHints hints = {0};
	if (min_width > 0 && min_height > 0) hints.flags |= PMinSize;

	hints.min_width = min_width;
	hints.min_height = min_height;
	XSetWMNormalHints(display, window, &hints);

	// Maps attributes and calls to window
	XMapWindow(display, window);
	XFlush(display);

	// create buffer
	//
	// pixels are 32 bits despite only needing 24 bits in order
	// to maintain alignment
	const int pixel_bits = 32;
	const int pixel_bytes = pixel_bits / 8;
	int pixel_buf_size = width * height * pixel_bytes;
	char *pixel_buf = malloc(pixel_buf_size);

	// buffer needs to be wrapped in X image structure so X can use it
	XImage *x_window_buffer = XCreateImage(
			display, visinfo.visual, visinfo.depth, 
			ZPixmap, 0, pixel_buf, width, height, pixel_bits, 0);
	GC default_gc = DefaultGC(display, default_screen);

	// store data
	data->display = display;
	data->window = window;
	data->x_window_buffer = x_window_buffer;
	data->default_gc = default_gc;
	data->visinfo = visinfo;

	data->width = width;
	data->height = height;
	data->pixel_bytes = pixel_bytes;
	data->pixel_buf_size = pixel_buf_size;
	data->pixel_buf = pixel_buf;
}


int main (int argc, char **argv)
{
	// init program state
	// state memory allocated, managed, and freed, by sp_plus
	void *sp_state = allocate_sp_state();
	if (!sp_state) {
		fprintf(stderr, "Error allocating state memory\n");
		exit(1);
	}

	// start audio callback
	// audio callback uses fill_audio_buffer declared in sp_plus.h
	snd_pcm_t *pcm = start_alsa(sp_state);


	// Setup X client
	struct x_window_data x_data = {0};
	init_x_window(&x_data);

	// request delete window message from window manager
	Atom WM_DELETE_WINDOW = XInternAtom(x_data.display, "WM_DELETE_WINDOW", 0);
	if (!XSetWMProtocols(x_data.display, x_data.window, &WM_DELETE_WINDOW, 1))
		fprintf(stderr, "Could not register WM_DELETE_WINDOW property\n");

	// maximize screen
	/*
	Atom wm_state = XInternAtom(x_data.display, "_NET_WM_STATE", 0);
	Atom max_h = XInternAtom(x_data.display, "_NET_WM_STATE_MAXIMIZED_HORZ", 0);
	Atom max_v = XInternAtom(x_data.display, "_NET_WM_STATE_MAXIMIZED_VERT", 0);

	if (wm_state == None) {
		fprintf(stderr, "failed to maximize window\n");
	} else {
		XClientMessageEvent ev = {0};
		ev.type = ClientMessage;
		ev.format = 32;
		ev.window = x_data.window;
		ev.message_type = wm_state;
		ev.data.l[0] = 2;
		ev.data.l[1] = max_h;
		ev.data.l[2] = max_v;
		ev.data.l[3] = 1;

		XSendEvent(	x_data.display, DefaultRootWindow(x_data.display), 
				0, SubstructureNotifyMask, (XEvent *) &ev);
	}
	*/

	// TODO performance tracking and maybe frame lock
	// main update and render loop
	int size_change = 0;
	int window_open = 1;
	while (window_open) {

		// handle window events
		XEvent ev = {0};
		while(XPending(x_data.display) > 0) {
			XNextEvent(x_data.display, &ev);
			switch(ev.type) {
				case DestroyNotify: 
					{
						// TODO no need to check window if
						// program is only using one window
						XDestroyWindowEvent *e = (XDestroyWindowEvent *) &ev;
						if (e->window == x_data.window) {
							window_open = 0;
						}
					} break;

				case ClientMessage:
					{
						XClientMessageEvent *e = (XClientMessageEvent *) &ev;
						if ((Atom) e->data.l[0] == WM_DELETE_WINDOW) {
							XDestroyWindow(x_data.display, x_data.window);
							window_open = 0;
						}
					} break;

				case ConfigureNotify:
					{
						XConfigureEvent *e = (XConfigureEvent *) &ev;
						x_data.width = e->width;
						x_data.height = e->height;
						size_change = 1;
					} break;

					// Get keyboard input
					// returning UTF-8 can be setup with XInput
				case KeyPress:
					{
						XKeyPressedEvent *e = (XKeyPressedEvent *) &ev;
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Left))
							printf("Left arrow pressed\n");
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Right))
							printf("Right arrow pressed\n");
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Up))
							printf("Up arrow pressed\n");
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Down))
							printf("Down arrow pressed\n");
					} break;

				case KeyRelease:
					{
						XKeyPressedEvent *e = (XKeyPressedEvent *) &ev;
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Left))
							printf("Left arrow released\n");
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Right))
							printf("Right arrow released\n");
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Up))
							printf("Up arrow released\n");
						if (e->keycode == XKeysymToKeycode(x_data.display, XK_Down))
							printf("Down arrow released\n");
					} break;
			}
		}

		// handle window size changes
		// if window size changes reallocate pixel_buf
		if (size_change) {
			size_change = 0;
			XDestroyImage(x_data.x_window_buffer);
			x_data.pixel_buf_size = x_data.width * x_data.height * x_data.pixel_bytes;
			x_data.pixel_buf = malloc(x_data.pixel_buf_size);

			x_data.x_window_buffer = XCreateImage(
					x_data.display, x_data.visinfo.visual, 
					x_data.visinfo.depth, ZPixmap, 0, 
					x_data.pixel_buf, x_data.width, x_data.height, 
					x_data.pixel_bytes * 8, 0);
		}

		// draw some temporary stuff to the screen
		/*
		const int pitch = x_data.width * x_data.pixel_bytes;
		for (int y = 0; y < x_data.height; y++) {
			char *row = x_data.pixel_buf + (y*pitch);
			for (int x = 0; x < x_data.width; x++) {
				unsigned int *p = (unsigned int *) (row + x * x_data.pixel_bytes);
				if (x % 16 && y % 16) *p = 0xffffffff;
				else *p = 0;
			}
		} */

		// call to sp_plus update and render service
		update_and_render_sp_plus(
				sp_state, x_data.pixel_buf, x_data.width,
				x_data.height, x_data.pixel_bytes);

		// blit pixel_buf to screen
		XPutImage(	x_data.display, x_data.window, x_data.default_gc,
				x_data.x_window_buffer, 0, 0, 0, 0, 
				x_data.width, x_data.height);
	}
	// TODO should close alsa handles, may prevent popping
	return 0;
}

long load_file(void **buffer, const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	// get size of file
	if (fseek(f, 0, SEEK_END)) {
		fclose(f);
		return 0;
	}
	const long size = ftell(f);
	if (size == -1) {
		fclose(f);
		return 0;
	}
	rewind(f);
	// allocate buffer
	*buffer = malloc(size);
	if (!*buffer) {
		fclose(f);
		return 0;
	}
	// read data to buffer
	if (!fread(*buffer, size, 1, f)) {
		free(*buffer);
		fclose(f);
		return 0;
	}

	fclose(f);
	return size;
}

void free_file_buffer(void **buffer) 
{ 
	if (*buffer) free(*buffer); 
}
