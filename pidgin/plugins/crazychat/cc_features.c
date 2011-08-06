#include <assert.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "cc_interface.h"
#include "crazychat.h"

#include "Utilities.h"
#include "QTUtilities.h"
#include "camdata.h"
#include "camproc.h"
#include "util.h"
#include <unistd.h>


#ifdef __APPLE_CC__
#include <Carbon/Carbon.h>
#else
#include <Carbon.h>
#endif

/* temporarily including for development testing */
#include "cc_gtk_gl.h"

/* hard coding the webcam dimensions: BAD, but we're not probing hardware yet */

#define WEBCAM_X		644	/* webcam's x dim */
#define WEBCAM_Y		480	/* webcam's y dim */

/* default webcam timer callback delay */
#define WEBCAM_CALLBACK_DELAY	40	/* in ms */

int x_click, y_click;
int mode_change=0;

struct input_instance input_data;

/* move this to input_instance eventually */
/*
UInt32 colorBuf[WEBCAM_Y][WEBCAM_X];
*/
unsigned int (*colorBuf)[640];
int detection_mode=0;
int draw_mode=0; //0=pixels, 1=face
int easter_count;
static void *kickstart(void *data);
static void *facefind(void *data);

/**
 * Resets the OpenGL viewport stuff on widget reconfiguration (resize,
 * reposition)
 * @param widget	widget that got reconfigured
 * @param event		the configuration event
 * @param data		unused
 * @return		TRUE ( i don't know what FALSE would do )
 */
static gboolean config_wrapper(GtkWidget *widget, GdkEventConfigure *event,
		void *data);

/**
 * Debug features test.  Draws pixels directly to the frame buffer.
 * @param widget	widget that we're drawing
 * @param event		the draw event
 * @param data		array of pixels
 * @return		DUNNO
 */
static gboolean mydraw(GtkWidget *widget, GdkEventExpose *event,
			      void *data);

/**
 * Periodically querys the webcam for more data.
 * @param instance	webcam input instance data
 * @return 		TRUE to stop other handler, FALSE to continue
 */
static gboolean webcam_cb(struct input_instance *instance);

/**
 * Init window code, adding our click callback.
 * @param widget	the window we clicked in
 * @param instance	webcam input instance data
 */
static void init_cb(GtkWidget *widget, struct input_instance *instance);

/**
 * Click callback
 * @param widget	the window we clicked in
 * @param event		the button click event structure
 * @param instance	input instance data
 * @return 		TRUE to stop other handler, FALSE to continue
 */
static gboolean click_cb(GtkWidget *widget, GdkEventButton *event,
		struct input_instance *instance);

/**
 * Button callback
 * @param button	the button we clicked on
 * @param instance	input instance data
 */
static void button_cb(GtkWidget *button, struct input_instance *instance);

/**
 * Destroy callback.  Called when the input processing window is destroyed.
 * @param widget	the window we clicked in
 * @param cc		crazychat global data structure
 */
static void destroy_cb(GtkWidget *widget, struct crazychat *cc);

/**
 * Set feature material.
 * @param entry		model selector combo box entry
 * @param material	pointer to material we're setting
 */
static void material_set(GtkWidget *entry, guint8 *material);

struct input_instance *init_input(struct crazychat *cc)
{

  /*pthread_t userinput_t; // should we put this in a nicer wrapper?*/
	struct draw_info *info;
	struct input_instance *instance;
	info = (struct draw_info*)malloc(sizeof(*info));
	assert(info);
	memset(info, 0, sizeof(*info));
	info->timeout = TRUE;
	info->delay_ms = DEFAULT_FRAME_DELAY;
	info->data = &input_data;
	instance = (struct input_instance*)info->data;
	memset(instance, 0, sizeof(*instance));
	instance->output.features = &instance->face;
	EnterMovies();
	filter_bank *bank;
	bank = Filter_Initialize();
	assert(CamProc(instance, bank) == noErr); // change this prototype-> no windows
	instance->timer_id = g_timeout_add(WEBCAM_CALLBACK_DELAY,
		(GSourceFunc)webcam_cb, instance);
	/*	THREAD_CREATE(&userinput_t, facefind, instance); // is this being created correctly?*/
	struct window_box ret;
	cc_new_gl_window(init_cb, config_wrapper, mydraw,
			info, &ret);
	instance->widget = ret.window;
	gtk_window_set_title(GTK_WINDOW(ret.window), "Local User");
	instance->box = ret.vbox;
	GtkWidget *label = gtk_label_new("Click your face");
	instance->label = label;
	gtk_box_pack_start(GTK_BOX(ret.vbox), label, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(ret.vbox), label, 0);
	gtk_widget_show(label);
	GtkWidget *button = gtk_button_new_with_label("Confirm");
	gtk_box_pack_start(GTK_BOX(ret.vbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_cb),
			instance);
	instance->button = button;
	gtk_widget_show(button);

	GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(ret.vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	GList *glist = NULL;
	glist = g_list_append(glist, "Dog");
	glist = g_list_append(glist, "Shark");
	instance->model = pidgin_text_combo_box_entry_new(NULL, glist);
	g_list_free(glist);
	//gtk_combo_box_set_column_span_column(GTK_COMBO(model_combo),
	//		10);
	gtk_box_pack_start(GTK_BOX(hbox), instance->model, TRUE, TRUE, 0);
	gtk_widget_show(instance->model);

	glist = NULL;
	glist = g_list_append(glist, "Red");
	glist = g_list_append(glist, "Dark Brown");
	glist = g_list_append(glist, "Light Brown");
	glist = g_list_append(glist, "White");
	glist = g_list_append(glist, "Green");
	glist = g_list_append(glist, "Black");
	instance->head = pidgin_text_combo_box_entry_new(NULL, glist);
	g_list_free(glist);
	//gtk_combo_box_set_column_span_column(GTK_COMBO(head_material_combo),
	//		10);
	gtk_box_pack_start(GTK_BOX(hbox), instance->head, TRUE, TRUE, 0);
	gtk_widget_show(instance->head);

	glist = NULL;
	glist = g_list_append(glist, "Red");
	glist = g_list_append(glist, "Dark Brown");
	glist = g_list_append(glist, "Light Brown");
	glist = g_list_append(glist, "White");
	glist = g_list_append(glist, "Green");
	glist = g_list_append(glist, "Black");
	instance->appendage = pidgin_text_combo_box_entry_new(NULL, glist);
	g_list_free(glist);
	//gtk_combo_box_set_column_span_column(GTK_COMBO(appendage_material_combo), 10);
	gtk_box_pack_start(GTK_BOX(hbox), instance->appendage,
			TRUE, TRUE, 0);
	gtk_widget_show(instance->appendage);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(ret.vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	glist = NULL;
	glist = g_list_append(glist, "Red");
	glist = g_list_append(glist, "Dark Brown");
	glist = g_list_append(glist, "Light Brown");
	glist = g_list_append(glist, "White");
	glist = g_list_append(glist, "Green");
	glist = g_list_append(glist, "Black");
	instance->lid = pidgin_text_combo_box_entry_new(NULL, glist);
	g_list_free(glist);
	//gtk_combo_box_set_column_span_column(GTK_COMBO(lids_material_combo), 10);
	gtk_box_pack_start(GTK_BOX(hbox), instance->lid, TRUE, TRUE, 0);
	gtk_widget_show(instance->lid);

	glist = NULL;
	glist = g_list_append(glist, "Red");
	glist = g_list_append(glist, "Dark Brown");
	glist = g_list_append(glist, "Light Brown");
	glist = g_list_append(glist, "White");
	glist = g_list_append(glist, "Green");
	glist = g_list_append(glist, "Black");
	instance->left_iris = pidgin_text_combo_box_entry_new(NULL, glist);
	g_list_free(glist);
	//gtk_combo_box_set_column_span_column(GTK_COMBO(left_iris_material_combo), 10);
	gtk_box_pack_start(GTK_BOX(hbox), instance->left_iris, TRUE, TRUE, 0);
	gtk_widget_show(instance->left_iris);

	/*
	glist = NULL;
	glist = g_list_append(glist, "Red");
	glist = g_list_append(glist, "Dark Brown");
	glist = g_list_append(glist, "Light Brown");
	glist = g_list_append(glist, "White");
	glist = g_list_append(glist, "Green");
	glist = g_list_append(glist, "Black");
	instance->right_iris = pidgin_text_combo_box_entry_new(NULL, glist);
	g_list_free(glist);
	//gtk_combo_box_set_column_span_column(GTK_COMBO(right_iris_material_combo), 10);
	gtk_box_pack_start(GTK_BOX(hbox), instance->right_iris, TRUE, TRUE, 0);
	gtk_widget_show(instance->right_iris);
*/
	gtk_widget_add_events(ret.draw_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(ret.draw_area), "button_press_event",
			G_CALLBACK(click_cb), instance);
	g_signal_connect(G_OBJECT(ret.window), "destroy",
			G_CALLBACK(destroy_cb),	cc);
//	gtk_widget_set_size_request(window, 640, 480);
	gtk_window_set_default_size(GTK_WINDOW(ret.window),320,300);


	GdkGeometry hints;
        hints.max_width = 640;
        hints.max_height = 480;

        gtk_window_set_geometry_hints (GTK_WINDOW(ret.window),
                                       NULL,
                                      &hints,
                                       GDK_HINT_MAX_SIZE);
	gtk_widget_show(ret.window);
	return instance;
}

static gboolean webcam_cb(struct input_instance *instance)
{
	assert(instance);
	QueryCam();
	return TRUE;
}

static void *facefind(void *data)
{
	fprintf(stderr, "waiting\n");
	getchar();
	fprintf(stderr,"got you");
	detection_mode=1;
	return;
}

void destroy_input(struct input_instance *instance)
{
	extern filter_bank *bank;
	assert(instance);
	Filter_Destroy(bank);
	g_source_remove(instance->timer_id);
	Die();
	ExitMovies();
}

static gboolean config_wrapper(GtkWidget *widget, GdkEventConfigure *event,
		void *data)
{


	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;
	GLfloat aspect;

//	fprintf(stderr,"Homicide  %f   %f  %d\n", w,h,draw_mode);

	if (draw_mode==1){
//		fprintf(stderr, "Bad place to be- actually not so bad\n");
		return configure(widget, event, data);
	}

	/*** OpenGL BEGIN ***/
	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
		return FALSE;


/* Undo all of the Model lighting here*/

//	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
//	glDisable(GL_CULL_FACE);
//	glDisable(GL_LIGHT0);
//	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
/* */


	glViewport(0,-(h/14),w*2,h*2);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0,0,640,640);
	glRasterPos2i(0,0);
	glPixelZoom(-w/(1*640),(-h/(1*480)));
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();




	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/

	return TRUE;
}

static gboolean mydraw(GtkWidget *widget, GdkEventExpose *event,
			      void *data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);
	struct input_instance *instance = (struct input_instance*)data;
	unsigned int *boo;
	struct cc_features *features = &instance->face;

	char *string = gtk_entry_get_text(GTK_COMBO(instance->model)->entry);
	if (!strcmp(string, "Dog")) {
		features->kind = 0;
	} else if (!strcmp(string, "Shark")) {
		features->kind = 1;
	}

	material_set(GTK_ENTRY(GTK_COMBO(instance->head)->entry),
			&features->head_color);
	material_set(GTK_ENTRY(GTK_COMBO(instance->appendage)->entry),
			&features->appendage_color);
	material_set(GTK_ENTRY(GTK_COMBO(instance->lid)->entry),
			&features->lid_color);
	material_set(GTK_ENTRY(GTK_COMBO(instance->left_iris)->entry),
			&features->left_iris_color);
	material_set(GTK_ENTRY(GTK_COMBO(instance->left_iris)->entry),
			&features->right_iris_color);

	if (easter_count>0) {
		easter_count--;
	} else {
		instance->face.mode = 0;
	}

	if (mode_change>0){
		mode_change--;
		config_wrapper(widget, event, data);
	}

	if (draw_mode==1){
		instance->output.my_output=LOCAL;
		return draw(widget,event,&instance->output);
	}


	boo = (unsigned int*)colorBuf;

	assert(instance);
	assert(gtk_widget_is_gl_capable(widget));

	/*** OpenGL BEGIN ***/

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
	//	fprintf(stderr, "We're fucked this time.\n");
		return FALSE;
	}

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawPixels(WEBCAM_X, WEBCAM_Y-70, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, boo);

	if (gdk_gl_drawable_is_double_buffered(gldrawable))
		gdk_gl_drawable_swap_buffers(gldrawable);
	else
		glFlush();

	gdk_gl_drawable_gl_end(gldrawable);

	/*** OpenGL END ***/

	return TRUE;
}

static void init_cb(GtkWidget *widget, struct input_instance *instance)
{
  //	setupDrawlists(LOCAL);
//	fprintf(stderr,"init_cb\n");
}

static gboolean click_cb(GtkWidget *widget, GdkEventButton *event,
		struct input_instance *instance)
{

	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;
	GLfloat aspect;

	if (draw_mode==1) {
		switch (event->button) {
			case 1:
				Debug("F U!\n");
				instance->face.mode = 1;
				easter_count = 5;
				break;
			case 3:
				Debug("should never get here\n");
				instance->face.mode = 2;
				easter_count = 5;
				break;
			default:
				instance->face.mode = 0;
				break;
		}
		return FALSE;
	}

	x_click=(event->x*(640/w));
	x_click=640-x_click;
	y_click=(event->y-(h/14))*(480/(h-(h/14)));
	detection_mode=1;
	//Debug("@@@ x:%d y:%d\n", x_click, y_click);

	gtk_label_set_text(instance->label,
			"Put on the box, then press confirm.");
	if (x_click <= 10) x_click=10;
	if (x_click >= WEBCAM_X-10) x_click=WEBCAM_X-60;
	if (y_click <= 10) y_click=10;
	if (y_click >= WEBCAM_Y-10) y_click=WEBCAM_Y-60;

	return FALSE;
}

static void button_cb(GtkWidget *button, struct input_instance *instance)
{
	if (!draw_mode) { /* transition to face mode */
		if (detection_mode == 0) { /* ignore confirm if no calibrate */
			return;
		}
		setupLighting(instance->widget);
		mode_change = 1;
		gtk_button_set_label(GTK_BUTTON(button), "Calibrate");
		gtk_label_set_label(instance->label,
				"If things get too crazy, click Calibrate.");
	} else { /* transition to calibration mode */
		gtk_label_set_label(instance->label, "Click your face");
		mode_change = 2;
		gtk_button_set_label(GTK_BUTTON(button), "Confirm");
	}
	draw_mode = !draw_mode;
}

static void destroy_cb(GtkWidget *widget, struct crazychat *cc)
{
	cc->features_state = 0;
	destroy_input(cc->input_data);
	cc->input_data = NULL;
}

static void material_set(GtkWidget *entry, guint8 *material)
{
	char *string = gtk_entry_get_text(GTK_ENTRY(entry));
	if (!strcmp(string, "Red")) {
		*material = 0;
	} else if (!strcmp(string, "Dark Brown")) {
		*material = 1;
	} else if (!strcmp(string, "Light Brown")) {
		*material = 2;
	} else if (!strcmp(string, "White")) {
		*material = 3;
	} else if (!strcmp(string, "Green")) {
		*material = 4;
	} else if (!strcmp(string, "Black")) {
		*material = 5;
	}
}
