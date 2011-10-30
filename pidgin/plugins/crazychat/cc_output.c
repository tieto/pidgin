#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
/*#include "util.h"*/
#include "cc_interface.h"
#include "crazychat.h"
#include <stdio.h>

#include "cc_gtk_gl.h"

#include "face.h"
#include "glm.h"


#define TAN_30 0.577
#define ROOT_3 1.73
//this will be the width of all heads
#define HEAD_WIDTH 2.5
#define HEAD_HEIGHT 2.5 //same for now, this will actually vary from head
#define PERSONS_HEAD 1
#define MOVEMENT .33

GLfloat ambient[] = {1.0, 1.0, 1.0, 1.0};
GLfloat diffuse[] = {.7, .7, .7, 1};
GLfloat specular[] = {1, 1, 1, 1};
GLfloat lightpos[] = {0.0, 0.0, 20, 1};
GLfloat specref[] = {1.0, 1, 1, 1,};

GLfloat xrot, yrot, zrot;
GLfloat x, y,  z, mouth_open;
GLint left_eye_frame, right_eye_frame;		//between 0 - 8
GLint mouth_type;
GLint mouth_frame;
DIRECTION dir;
BOOL left_open, right_open;
GLfloat window_aspect;

FACE remote_shark_face, remote_dog_face, local_shark_face, local_dog_face;
int count = 0;
int curr_materials[DOG_SHARK_CHANGE]; //these are the materials from output_instance
OUTPUT_MODE curr_mode;
KIND which_face;

static void destroy_cb(GtkWidget *widget, struct output_instance *data);

void Interpolate(struct output_instance* instance){
	GLfloat rangeX, rangeY, minZ, adj, angle, temp;
	count++;
/*	yrot=90;
	zrot=0;
	z=5;
	x=0;
	y=0;
	left_open = right_open = TRUE;
	mouth_open = (float)(count%10)/10;
	dir = CONST;
	curr_mode = NORMAL;
	return;
*/

	//find z plane from percentage of face
	if(instance->features->head_size==0){
		z = 5;
	}

	temp = (GLfloat)instance->features->head_size/40.0;
	//printf("head size %d\n", instance->features->head_size);

	minZ = ROOT_3;
	z = ROOT_3*(PERSONS_HEAD/temp);
	if(z < minZ)
		z = minZ;

	//these calculations are based on a 90 degree viewing angle
	rangeX = z*(TAN_30)*2;
	rangeY = window_aspect*rangeX;
	temp = (GLfloat)instance->features->x;
	if(temp>50) { //then its on the left
		temp = (temp - 50.0)/50.0;
		x = 0 - temp*rangeX/1.0;
	}
	else {
		temp = (50.0-temp)/50.0;
		x = 0 + temp*rangeX/1.0;
	}

	temp = (GLfloat)instance->features->y;

	if(temp>50){
		temp = (temp-50.0)/50.0;
		y = 0 - temp*rangeY/1.0;
	}
	else {
		temp = (50.0-temp)/50.0;
		y = 0 + temp*rangeY/1.0;
	}

	temp = (GLfloat)instance->features->head_y_rot;
	yrot = temp - 50;
	temp = (GLfloat)instance->features->head_z_rot;
	zrot = temp-50;

	if(y-instance->past_y < -MOVEMENT)
	  dir = DOWN;
	else if(y-instance->past_y > MOVEMENT)
	  dir = UP;
	else
	  dir = CONST;
	instance->past_y=y;

	mouth_open = (float)instance->features->mouth_open/105;
	count++;
	//mouth_open = (count%10)/(10);

	if(instance->features->left_eye_open==0){
		left_open = FALSE;
	}
	else{
		left_open = TRUE;
	}

	if(instance->features->right_eye_open==0)
		right_open = FALSE;
	else
		right_open = TRUE;
	//right_open =1 - (count%5)/4;

	//set the materials
	curr_materials[APPENDAGE]=instance->features->appendage_color;
	curr_materials[HEAD]=instance->features->head_color;
	curr_materials[LIDS]=instance->features->lid_color;
	curr_materials[LEFT_IRIS]=instance->features->left_iris_color;
	curr_materials[RIGHT_IRIS]=instance->features->right_iris_color;
	// we don't get an x rotation
	xrot = 0;
	curr_mode=instance->features->mode;
	if(instance->features->kind==0)
		which_face=DOG;
	else
		which_face=SHARK;

}



gboolean configure(GtkWidget *widget,
				GdkEventConfigure *event, void *data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;
	GLfloat aspect;

	Debug("configuring\n");


	/*** OpenGL BEGIN ***/
	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
		return FALSE;

	glEnable(GL_DEPTH_TEST);
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	if (w > h) {
		window_aspect = w / h;
	} else {
		window_aspect = h / w;
	}

	//glOrtho(-10, 10, -10,10, 0.0001, 1000);
	gluPerspective(90.0, window_aspect, 0.0001, 1000.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/

	return TRUE;
}

gboolean draw(GtkWidget *widget, GdkEventExpose *event,
			      void *data)
{
	struct output_instance *instance = (struct output_instance*)data;
	if(!data) {
		fprintf(stderr,"null\n");
	}
	assert(instance);
	Interpolate(instance);

	GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);

	//return TRUE;

	assert(gtk_widget_is_gl_capable(widget));

	if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext)) {
		fprintf(stderr, "We're fucked this time.\n");
		return FALSE;
	}


	glClearColor(1.0, 1.0, 1.0, 0.0);
	//glDisable(GL_CULL_FACE);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor3f(1.0, 1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(x, y, -z);
	if(instance->my_output==LOCAL){
		if(which_face==DOG){
			change_materials(local_dog_face, curr_materials, DOG_SHARK_CHANGE);
			draw_face(local_dog_face, zrot, yrot, left_open, right_open, mouth_open, dir, curr_mode);
		}
		else {
			change_materials(local_shark_face, curr_materials, DOG_SHARK_CHANGE);
			draw_face(local_shark_face, zrot, yrot, left_open, right_open, mouth_open, dir, curr_mode);
		}
	}
	else{
		if(which_face==DOG){
			change_materials(remote_dog_face, curr_materials, DOG_SHARK_CHANGE);
			draw_face(remote_dog_face, zrot, yrot, left_open, right_open, mouth_open, dir, curr_mode);
		}
		else{
			change_materials(remote_shark_face, curr_materials, DOG_SHARK_CHANGE);
			draw_face(remote_shark_face, zrot, yrot, left_open, right_open, mouth_open, dir, curr_mode);
		}
	}
	if (gdk_gl_drawable_is_double_buffered (gldrawable))
		gdk_gl_drawable_swap_buffers (gldrawable);
	else
		glFlush ();
	return TRUE;
}

void init (GtkWidget *widget, void *data)
{
	setupDrawlists(REMOTE);
	setupLighting(widget);
}


void setupDrawlists(OUTPUT output)
{
	if(output==REMOTE){
		remote_shark_face = init_face(SHARK);
		remote_dog_face = init_face(DOG);
	}
	if(output==LOCAL){
		local_shark_face = init_face(SHARK);
		local_dog_face = init_face(DOG);
	}
}


void setupLighting(GtkWidget *widget)
{

	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        //glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
        glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
        glEnable(GL_LIGHT0);
        //glEnable(GL_COLOR_MATERIAL);
        //glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
        glMateriali(GL_FRONT, GL_SHININESS, 128);

        //glEnable(GL_CULL_FACE);

        glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//glOrtho(-2, 2, -2, 2, 0.0001, 1000);

	if (w > h) {
		window_aspect = w / h;
	} else {
		window_aspect = h / w;
	}

	gluPerspective(90.0, window_aspect, 0.0001, 1000.0);
	//glFrustum(-100.0, 100.0, -100.0, 100.0, 0, 10);
	glMatrixMode(GL_MODELVIEW);
	//gluLookAt(0.0, 0.0, 5.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
}

struct output_instance *init_output(struct cc_features *features,
		struct cc_session *session)
{
	struct draw_info *info;
	struct output_instance *instance;
	struct window_box ret;

	instance = (struct output_instance*)malloc(sizeof(*instance));
	assert(instance);
	memset(instance, 0, sizeof(*instance));
	instance->features = features;
	instance->session = session;

	info = (struct draw_info*)malloc(sizeof(*info));
	memset(info, 0, sizeof(*info));
	info->timeout = TRUE;
	info->delay_ms = 40;
	info->data = instance;


	cc_new_gl_window(init, configure, draw, info, &ret);
	g_signal_connect(GTK_OBJECT(ret.window), "destroy",
			G_CALLBACK(destroy_cb),	instance);
	gtk_window_set_default_size(ret.window,640,480);
	if (session) {
		gtk_window_set_title(ret.window, session->name);
	}
	gtk_widget_show(ret.window);
	instance->widget = ret.window;
	instance->box = ret.vbox;
	return instance;
}

static void destroy_cb(GtkWidget *widget, struct output_instance *data)
{
	Debug("Closing output window\n");
	if (data->session) {
		cc_remove_session(data->session->cc, data->session);
		close(data->session->tcp_sock);
		close(data->session->udp_sock);
		Filter_Destroy(data->session->filter);
		destroy_output(data->session->output);
	}
}

void destroy_output(struct output_instance *instance)
{
	free(instance);
}
