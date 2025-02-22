#include "../sp_plus.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "linux_audio.c"

#define NSEC_PER_SEC 1000000000
#define NSEC_PER_MS 1000000

/* X11 implementation */

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

// converts X keycode to sp_plus Key type
static int key_x_to_sp(XKeyEvent *ev)
{
	switch (XLookupKeysym(ev, 1)) {
		case XK_A:
			return KEY_A;
		case XK_B:
			return KEY_B;
		case XK_C:
			return KEY_C;
		case XK_D:
			return KEY_D;
		case XK_E:
			return KEY_E;
		case XK_F:
			return KEY_F;
		case XK_G:
			return KEY_G;
		case XK_H:
			return KEY_H;
		case XK_I:
			return KEY_I;
		case XK_J:
			return KEY_J;
		case XK_K:
			return KEY_K;
		case XK_L:
			return KEY_L;
		case XK_M:
			return KEY_M;
		case XK_N:
			return KEY_N;
		case XK_O:
			return KEY_O;
		case XK_P:
			return KEY_P;
		case XK_Q:
			return KEY_Q;
		case XK_R:
			return KEY_R;
		case XK_S:
			return KEY_S;
		case XK_T:
			return KEY_T;
		case XK_U:
			return KEY_U;
		case XK_V:
			return KEY_V;
		case XK_W:
			return KEY_W;
		case XK_X:
			return KEY_X;
		case XK_Y:
			return KEY_Y;
		case XK_Z:
			return KEY_Z;
		case XK_minus:
			return KEY_MINUS;
		case XK_equal:
			return KEY_EQUAL;
		case XK_Shift_L:
			return KEY_SHIFT_L;
		case XK_Shift_R:
			return KEY_SHIFT_R;
		default:
			return -1;
	}
}
// converts sp_plus Key type to X keycode
static int key_sp_to_x (Display *d, int key)
{
	switch (key) {
		case KEY_A:
			return XKeysymToKeycode(d, XK_A);
		case KEY_B:
			return XKeysymToKeycode(d, XK_B);
		case KEY_C:
			return XKeysymToKeycode(d, XK_C);
		case KEY_D:
			return XKeysymToKeycode(d, XK_D);
		case KEY_E:
			return XKeysymToKeycode(d, XK_E);
		case KEY_F:
			return XKeysymToKeycode(d, XK_F);
		case KEY_G:
			return XKeysymToKeycode(d, XK_G);
		case KEY_H:
			return XKeysymToKeycode(d, XK_H);
		case KEY_I:
			return XKeysymToKeycode(d, XK_I);
		case KEY_J:
			return XKeysymToKeycode(d, XK_J);
		case KEY_K:
			return XKeysymToKeycode(d, XK_K);
		case KEY_L:
			return XKeysymToKeycode(d, XK_L);
		case KEY_M:
			return XKeysymToKeycode(d, XK_M);
		case KEY_N:
			return XKeysymToKeycode(d, XK_N);
		case KEY_O:
			return XKeysymToKeycode(d, XK_O);
		case KEY_P:
			return XKeysymToKeycode(d, XK_P);
		case KEY_Q:
			return XKeysymToKeycode(d, XK_Q);
		case KEY_R:
			return XKeysymToKeycode(d, XK_R);
		case KEY_S:
			return XKeysymToKeycode(d, XK_S);
		case KEY_T:
			return XKeysymToKeycode(d, XK_T);
		case KEY_U:
			return XKeysymToKeycode(d, XK_U);
		case KEY_V:
			return XKeysymToKeycode(d, XK_V);
		case KEY_W:
			return XKeysymToKeycode(d, XK_W);
		case KEY_X:
			return XKeysymToKeycode(d, XK_X);
		case KEY_Y:
			return XKeysymToKeycode(d, XK_Y);
		case KEY_Z:
			return XKeysymToKeycode(d, XK_Z);
		case KEY_MINUS:
			return XKeysymToKeycode(d, XK_minus);
		case KEY_EQUAL:
			return XKeysymToKeycode(d, XK_equal);
		case KEY_SHIFT_L:
			return XKeysymToKeycode(d, XK_Shift_L);
		case KEY_SHIFT_R:
			return XKeysymToKeycode(d, XK_Shift_R);
		default:
			return -1;
	}
}

// initializes a window and populates data struct
// data struct will be initialized inside init_x_window
void init_x_window(struct x_window_data *data)
{
	// TODO store these somewhere better
	// create display connection
	const int width = 1920;
	const int height = 1080;

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
	attributes.colormap = XCreateColormap(display, root, visinfo.visual, AllocNone);
	// attributes.event_mask = StructureNotifyMask | KeyPressMask;
	attributes.event_mask = KeyPressMask;
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
	// lock window fullscreen for now
	const int min_width = 1920;
	const int min_height = 1080;
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



/* update and render loop lives here
 * calls sp_plus services to get audio visual output */

int main (int argc, char **argv)
{
	// TODO Error logging for thes init functions
	// init program state
	// state memory allocated, managed, and freed, by sp_plus
	void *sp_state = sp_plus_allocate_state();
	if (!sp_state) {
		fprintf(stderr, "Error allocating state memory\n");
		exit(1);
	}

	// start audio callback
	// audio callback uses fill_audio_buffer declared in sp_plus.h
	snd_pcm_t *pcm = start_alsa(sp_state);
	if (!pcm) {
		fprintf(stderr, "Error starting audio\n");
		// TODO: Devide what to did if audio is not started
		// always exiting is annoying for debuggin

		//exit(1);
	}



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

	/* main update and render loop */

	// frame cap data
	const int target_fps = 30;
	const long target_npf = (long) NSEC_PER_SEC / target_fps;
	struct timespec start_time_rt;
	clock_gettime(CLOCK_REALTIME, &start_time_rt);

	// window flags
	// int size_change = 0;
	int window_open = 1;

	// bitfields to track key press state
	char last_keystate[32];
	XQueryKeymap(x_data.display, last_keystate);

	while (window_open) {


		/* keyboard input */
		struct key_input input = {0};
		char curr_keystate[32];
		XQueryKeymap(x_data.display, curr_keystate);

		for (int sp_key = 0; sp_key < KEY_SHIFT_R; sp_key++) {

			const int keycode = key_sp_to_x(x_data.display, sp_key);
			if (keycode < 0 || keycode >= 256) continue;

			const int i = keycode / 8;
			const int state_mask = 0b1 << (keycode % 8);

			// if key is down
			if (curr_keystate[i] & state_mask) {
				if (!(last_keystate[i] & state_mask)){
					input.key_pressed |= 0b1 << sp_key;
				}
				input.key_down |= 0b1 << sp_key;
			}
			// if key is up
			else if (last_keystate[i] & state_mask && curr_keystate[i] ^ state_mask) {
				input.key_released |= 0b1 << sp_key;
			}

		}
		memcpy(last_keystate, curr_keystate, 32);


		/* window events */
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

					/*
				case ConfigureNotify:
					{
						XConfigureEvent *e = (XConfigureEvent *) &ev;
						x_data.width = e->width;
						x_data.height = e->height;
						size_change = 1;
					} break;

				*/
				case KeyPress:
					// counts num key presses, does NOT confirm key is down
					{
						XKeyPressedEvent *e = (XKeyPressedEvent *) &ev;
						int key = key_x_to_sp(e);
						if (key != -1) input.num_key_press[key]++;
					} break;
			}
		}


		// handle window size changes
		// if window size changes reallocate pixel_buf
		/*
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
		*/

		// call to sp_plus update and render service
		sp_plus_update_and_render(
				sp_state, x_data.pixel_buf, x_data.width,
				x_data.height, x_data.pixel_bytes, &input);

		// blit pixel_buf to screen
		XPutImage(	x_data.display, x_data.window, x_data.default_gc,
				x_data.x_window_buffer, 0, 0, 0, 0, 
				x_data.width, x_data.height);

		/* enforce frame cap */
		struct timespec req;
		req.tv_sec = start_time_rt.tv_sec;
		req.tv_nsec = start_time_rt.tv_nsec + target_npf;
		if (req.tv_nsec > 999999999) {
			req.tv_sec += req.tv_nsec / NSEC_PER_SEC;
			req.tv_nsec = req.tv_nsec % NSEC_PER_SEC;
		}

		// sleep until request time is reached then reset start_time
		while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &req, NULL));


		// TODO: performance queury and frame cap implementation kind junk.
		// 	 figure out how to improve the design

		/* output framerate after enforcing frame cap (Coarse) */
		/*
		   struct timespec end_time;
		   clock_gettime(CLOCK_REALTIME, &end_time);
		// time elapsed in nano seconds
		const long end_time_nsec = end_time.tv_sec * NSEC_PER_SEC + end_time.tv_nsec;
		const long start_time_nsec = start_time_rt.tv_sec * NSEC_PER_SEC + start_time_rt.tv_nsec;
		const long elapsed_time_nsec = end_time_nsec - start_time_nsec;
		start_time_rt = end_time;

		// output timing data for debug, low precision
		printf(		"%dms/f, %df/s\n", 
		(int) (elapsed_time_nsec / NSEC_PER_MS), 
		(int) (NSEC_PER_SEC / elapsed_time_nsec));
		*/

		clock_gettime(CLOCK_REALTIME, &start_time_rt);
	}
	// TODO should close alsa handles, may prevent popping
	return 0;
}

/* File IO services used by sp_plus service */

long platform_load_entire_file(void **buffer, const char *path)
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

void platform_free_file_buffer(void **buffer) 
{ 
	if (*buffer) free(*buffer); 
}
