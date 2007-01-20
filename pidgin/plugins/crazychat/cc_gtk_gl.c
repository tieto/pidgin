#include <assert.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "cc_gtk_gl.h"
#include "util.h"

static GdkGLConfig *glconfig = NULL;

/**
 * Resets the OpenGL viewport stuff on widget reconfiguration (resize,
 * reposition)
 * @param widget	widget that got reconfigured
 * @param event		the configuration event
 * @param data		unused
 * @return		FALSE to propagate other handlers
 */
static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event,
		void *data);

/**
 * Maps the widget to the screen.
 * @param widget	widget that got mapped
 * @param event		the map event
 * @param data		draw info struct
 * @return		FALSE to propagate other handlers
 */
static gboolean map_event(GtkWidget *widget, GdkEventAny *event, void *data);

/**
 * Unmaps the widget from the screen.
 * @param widget	widget that got unmapped
 * @param event		the configuration event
 * @param data		draw info struct
 * @return		FALSE to propagate other handlers
 */
static gboolean unmap_event(GtkWidget *widget, GdkEventAny *event, void *data);

/**
 * Respond to widget visibility change.
 * @param widget	widget whose visibility changed
 * @param event		the visibility event
 * @param data		draw info struct
 * @return		FALSE to propagate other handlers
 */
static gboolean visibility_notify_event(GtkWidget *widget,
		GdkEventVisibility *event, void *data);

/**
 * Add a glib timer to periodically draw the widget.
 * @param widget	widget we're drawing
 * @param info		draw info struct
 */
static void widget_draw_timer_add(GtkWidget *widget, struct draw_info *info);

/**
 * Remove glib timer that was drawing this widget.
 * @param widget	widget we're drawing
 * @param info		draw info struct
 */
static void widget_draw_timer_remove(GtkWidget *widget, struct draw_info *info);

/**
 * Periodically invalidates gtk gl widget and tells GTK to redraw
 * @param widget	widget we're drawing
 */
static gboolean widget_draw_timer(GtkWidget *widget);

/**
 * Cleanup widget stuff when it's getting destroyed.
 * @param widget	widget that got destroyed
 * @param data		draw info struct
 */
static void destroy_event(GtkWidget *widget, struct draw_info *data);

int cc_init_gtk_gl()
{
	if (glconfig)
		return 0;

	/* configure OpenGL */

	glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB |
					     GDK_GL_MODE_DEPTH |
					     GDK_GL_MODE_DOUBLE);

	if (glconfig == NULL) {
		Debug("*** Cannot find the double-buffered visual.\n");
		Debug("*** Trying single-buffered visual.\n");

		/* Try single-buffered visual */
		glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB |
						     GDK_GL_MODE_DEPTH);
		if (glconfig == NULL) {
			Debug("*** No appropriate OpenGL-capable visual "
			      "found.\n");
			return 1;
		}
	}

	return 0;
}

void cc_new_gl_window(gl_init_func init, gl_config_func config,
		gl_draw_func draw, struct draw_info *data,
		struct window_box *ret)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *drawing_area;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_reallocate_redraws(GTK_CONTAINER(window), TRUE);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	if (!data) {
		data = (struct draw_info*)malloc(sizeof(*data));
		assert(data);
		memset(data, 0, sizeof(*data));
		data->timeout = TRUE;
		data->delay_ms = DEFAULT_FRAME_DELAY;
	}
	drawing_area = cc_new_gl_area(init, config, draw, data);
	gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
	gtk_widget_show(drawing_area);
	ret->window = window;
	ret->vbox = vbox;
	ret->draw_area = drawing_area;
}

GtkWidget *cc_new_gl_area(gl_init_func init, gl_config_func config,
		gl_draw_func draw, struct draw_info *data)
{
	GtkWidget *drawing_area;

	assert(data);
	
	drawing_area = gtk_drawing_area_new();
	assert(drawing_area);

	assert(gtk_widget_set_gl_capability(drawing_area, glconfig, NULL, FALSE,
				     GDK_GL_RGBA_TYPE));
	gtk_widget_add_events (drawing_area, GDK_VISIBILITY_NOTIFY_MASK);
	if (init) {
		g_signal_connect_after(G_OBJECT(drawing_area), "realize",
			G_CALLBACK(init), data->data);
	}
	if (config) {
		g_signal_connect(G_OBJECT(drawing_area), "configure_event",
			G_CALLBACK(config), NULL);
	} else {
		g_signal_connect(G_OBJECT(drawing_area), "configure_event",
			G_CALLBACK(configure_event), NULL);
	}
	if (draw) {
		g_signal_connect(G_OBJECT(drawing_area), "expose_event",
			G_CALLBACK(draw), data->data);
	}
	g_signal_connect(G_OBJECT(drawing_area), "map_event",
			G_CALLBACK(map_event), data);
	g_signal_connect(G_OBJECT(drawing_area), "unmap_event",
			G_CALLBACK(unmap_event), data);
	g_signal_connect(G_OBJECT(drawing_area), "visibility_notify_event",
			G_CALLBACK(visibility_notify_event), data);
	g_signal_connect(G_OBJECT(drawing_area), "destroy",
			G_CALLBACK(destroy_event), data);

	return drawing_area;
}


static gboolean configure_event(GtkWidget *widget,
				GdkEventConfigure *event, void *data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;
	GLfloat aspect;

//	Debug("configuring\n");
	
	/*** OpenGL BEGIN ***/
	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
		return FALSE;

	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);

	glLoadIdentity();
	if (w > h) {
		aspect = w / h;
		glFrustum(-aspect, aspect, -1.0, 1.0, 2.0, 60.0);
	} else {
		aspect = h / w;
		glFrustum(-1.0, 1.0, -aspect, aspect, 2.0, 60.0);
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/

	return FALSE;
}

static int map_event(GtkWidget *widget, GdkEventAny *event, void *data)
{
	struct draw_info *info = (struct draw_info*)data;
	Debug("map\n");
	
	if (info->timeout) {
		widget_draw_timer_add(widget, info);
	}
	return FALSE;
}

static int unmap_event(GtkWidget *widget, GdkEventAny *event, void *data)
{
	struct draw_info *info = (struct draw_info*)data;
	Debug("unmap\n");
	
	if (info->timeout) {
		widget_draw_timer_remove(widget, info);
	}
	return FALSE;
}

static int visibility_notify_event(GtkWidget *widget, GdkEventVisibility *event,
		void *data)
{
	struct draw_info *info = (struct draw_info*)data;
	Debug("visibility\n");
	
	if (event->state == GDK_VISIBILITY_FULLY_OBSCURED) {
		Debug("obscured\n");
		if (info->timeout) {
			widget_draw_timer_remove(widget, info);
		}
	} else {
		Debug("visible\n");
		if (info->timeout) {
			widget_draw_timer_add(widget, info);
		}
	}
	return FALSE;
}

static void widget_draw_timer_add(GtkWidget *widget, struct draw_info *info)
{
	if (!info->timer_id) {
		info->timer_id = g_timeout_add(info->delay_ms,
				(GSourceFunc)widget_draw_timer, widget);
	}
}

static void widget_draw_timer_remove(GtkWidget *widget, struct draw_info *info)
{
	if (info->timer_id) {
		g_source_remove(info->timer_id);
		info->timer_id = 0;
	}
}

static gboolean widget_draw_timer(GtkWidget *widget)
{
	/* invalidate the window */
	gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);

	/* tell gtk to update it _now_ */
	gdk_window_process_updates (widget->window, FALSE);

	return TRUE;
}

static void destroy_event(GtkWidget *widget, struct draw_info *data)
{
	Debug("destroying widget\n");
	
	if (data) {
		widget_draw_timer_remove(widget, data);
		free(data);
	}
}
