#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

#define DEFAULT_FRAME_DELAY		40

typedef void (gl_init_func) (GtkWidget *widget, void *data);
typedef gboolean (*gl_config_func) (GtkWidget *widget, GdkEventConfigure *event,
		void *data);
typedef gboolean (*gl_draw_func) (GtkWidget *widget, GdkEventExpose *event,
			      void *data);

struct draw_info {
	gboolean timeout;		/* use/not use a timer callback */
	int timer_id;			/* glib timer callback id */
	guint delay_ms;			/* timer callback delay in ms */
	void *data;			/* drawing data */
};

struct window_box {
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *draw_area;
};

/**
 * Initialize the gtkglext framework for all our widgets.
 * @return		0 on success, non-zero on failure
 */
int cc_init_gtk_gl(void);

/**
 * Create a new OpenGL enabled window
 * @param init		the initialize callback function
 * @param draw		the drawing callback function
 * @param data		drawing metadata
 * @param ret		struct with returned window and vbox
 */
void cc_new_gl_window(gl_init_func init, gl_config_func config,
		gl_draw_func draw, struct draw_info *data,
		struct window_box *ret);

/**
 * Create a new OpenGL enabled drawing area widget.
 * @param init		the initialize callback function
 * @param draw		the drawing callback function
 * @param data		drawing metadata
 * @return 		the drawing widget
 */
GtkWidget *cc_new_gl_area(gl_init_func init, gl_config_func config,
		gl_draw_func draw, struct draw_info *data);
