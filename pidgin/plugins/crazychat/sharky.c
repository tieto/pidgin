#include "face.h"
#include "sharky.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include "models.h"

#define NUM_SHARKS 12
#define NUM_SHARK_LIDS 10
#define NUM_FINS 4
#define NUM_SHARK_EYES 1
#define NUM_SHARK_PUPILS 1
#define NUM_SHARK_IRIS 1
#define SHARK_SCALE .025
#define SHARK_EYES_Y 7.5*SHARK_SCALE // .295
#define SHARK_EYES_X 28.89*SHARK_SCALE // .26
#define SHARK_EYES_Z 56*SHARK_SCALE // .38
#define SHARK_FINS_X 75*SHARK_SCALE //.65
#define SHARK_FINS_Y -44*SHARK_SCALE//.18
#define SHARK_FINS_Z -15*SHARK_SCALE //.05
#define SHARK_IRIS_X 0*SHARK_SCALE
#define SHARK_IRIS_Y 1.67*SHARK_SCALE//.015
#define SHARK_IRIS_Z 5.0*SHARK_SCALE//.08
#define SHARK_PUP_X 0*SHARK_SCALE
#define SHARK_PUP_Y 0*SHARK_SCALE
#define SHARK_PUP_Z 2.5*SHARK_SCALE //.028
#define SHARK_IRIS_SCALE .10*SHARK_SCALE
#define SHARK_PUP_SCALE .08*SHARK_SCALE
#define SHARK_FIN_SCALE .9*SHARK_SCALE
#define SHARK_EYE_SCALE .7*SHARK_SCALE
#define SHARK_LID_SCALE .84*SHARK_SCALE
#define SHARK_HEAD_SCALE .58*SHARK_SCALE
#define TOP_FIN_X 0*SHARK_SCALE
#define TOP_FIN_Y 4*SHARK_SCALE
#define TOP_FIN_Z 25*SHARK_SCALE
#define BOT_FIN_X 0*SHARK_SCALE
#define BOT_FIN_Y 9*SHARK_SCALE
#define BOT_FIN_Z -70*SHARK_SCALE
#define TOP_FIN_SCALE 2*SHARK_SCALE
#define MAX_FIN_ANGLE 90.0
#define MIN_FIN_ANGLE -20.0
#define ANGLE_INC 60
float fins=0;


char shark_mtl_file[MAX_FILE_LEN] = "dog.mtl";
// the initial dog materials
GLint init_shark_mats[NUM_PARTS] = {1, 2, 3, 4, 5, 3, 5, 0, 0, 0};

void draw_shark_pupil(FACE f, PART p) {
	struct shark_struct* shark=(struct shark_struct*)f->char_struct;
	glPushMatrix();
	glTranslatef(SHARK_IRIS_X, -SHARK_IRIS_Z, SHARK_IRIS_Y);
	if(p==LEFT_IRIS)
		glmSetMat(f->materials, f->mat_indeces[LEFT_IRIS]);
	else
		glmSetMat(f->materials, f->mat_indeces[RIGHT_IRIS]);
	glCallList(shark->iris[0]);
	glTranslatef(SHARK_PUP_X, -SHARK_PUP_Z, SHARK_PUP_Y);
	glmSetMat(f->materials, f->mat_indeces[PUPIL]);
	glCallList(shark->pupil[0]);
	glPopMatrix();
}

void draw_shark_left_eye(FACE f, BOOL open, int max) {
	struct shark_struct* shark=(struct shark_struct*)f->char_struct;
	if(f->my_mode==CRAZY2)
		shark->curr_left_lid=NUM_SHARK_LIDS-1;
	else
		shark->curr_left_lid = compute_lid(open, shark->curr_left_lid, max);
	glPushMatrix();
	glTranslatef(-SHARK_EYES_X, 0.0, 0.0);
	glPushMatrix();
	glTranslatef(0.0, -f->curr_eye_pop, 0.0);
	draw_shark_pupil(f, LEFT_IRIS);
	glmSetMat(f->materials, f->mat_indeces[EYES]);
	glCallList(shark->eyes[shark->curr_left_eye]);
	glPopMatrix();
	glmSetMat(f->materials, f->mat_indeces[LIDS]);
	glCallList(shark->lids[shark->curr_left_lid]);
	glPopMatrix();
}

void draw_shark_right_eye(FACE f, BOOL open, int max) {
	struct shark_struct* shark=(struct shark_struct*)f->char_struct;
	if(f->my_mode==CRAZY2)
		shark->curr_right_lid=NUM_SHARK_LIDS-1;
	else
		shark->curr_right_lid = compute_lid(open, shark->curr_right_lid, max);
	glPushMatrix();
	glTranslatef(SHARK_EYES_X, 0.0, 0.0);
	glPushMatrix();
	glTranslatef(0.0, -f->curr_eye_pop, 0.0);
	draw_shark_pupil(f, RIGHT_IRIS);
	glmSetMat(f->materials, f->mat_indeces[EYES]);
	glCallList(shark->eyes[shark->curr_right_eye]);
	glPopMatrix();
	glmSetMat(f->materials, f->mat_indeces[LIDS]);
	glCallList(shark->lids[shark->curr_right_lid]);
	glPopMatrix();
}

void shark_eyes(FACE f, GLfloat angle, GLfloat yangle, BOOL left_open, BOOL right_open, DIRECTION dir)
{
	struct shark_struct* shark=(struct shark_struct*)f->char_struct;
	int max_eye;
	if(dir==CONST) { //then not moving, eyes are gettin sleepy
		f->eye_count--;
	}
	else{
		f->eye_count=EYE_TIME*NUM_SHARK_LIDS-1;
	}
	max_eye=f->eye_count/EYE_TIME;
	if(max_eye<0)
		max_eye=0;
	if(f->my_mode==CRAZY2)
		f->curr_eye_pop=f->curr_eye_pop + (MAX_EYE_POP - f->curr_eye_pop)/2;
	else
		f->curr_eye_pop=f->curr_eye_pop - (f->curr_eye_pop-0)/2;
	glPushMatrix();
	glTranslatef(0, 0, SHARK_EYES_Y);
	glTranslatef(0, -SHARK_EYES_Z,0);
	draw_shark_left_eye(f, left_open, max_eye);
	draw_shark_right_eye(f, right_open, max_eye);
	glPopMatrix();
}

void shark_fins(FACE f, DIRECTION dir){
	struct shark_struct* shark=(struct shark_struct*)f->char_struct;

	if(dir==DOWN){
		if(shark->curr_fin < (NUM_FINS-1))
			shark->curr_fin++;
		shark->curr_fin_angle = shark->curr_fin_angle+(MAX_FIN_ANGLE-shark->curr_fin_angle)/2;
	}
	if(dir==UP){
		if(shark->curr_fin > 0)
			shark->curr_fin--;
		shark->curr_fin_angle = shark->curr_fin_angle+(MIN_FIN_ANGLE-shark->curr_fin_angle)/2;
	}
	else if(dir==CONST){
		shark->curr_fin=1;
		shark->curr_fin_angle = shark->curr_fin_angle+(0-shark->curr_fin_angle)/3;
	}

	glPushMatrix();
	glTranslatef(-SHARK_FINS_X, -SHARK_FINS_Z, SHARK_FINS_Y);
	if(f->my_mode==CRAZY1)
		glRotatef(MAX_FIN_ANGLE, 0.0, 1.0, 0.0);
	else
		glRotatef(shark->curr_fin_angle, 0.0, 1.0, 0.0);
	glmSetMat(f->materials, f->mat_indeces[APPENDAGE]);
	glCallList(shark->fins[shark->curr_fin]);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(SHARK_FINS_X, -SHARK_FINS_Z, SHARK_FINS_Y);
	glScalef(-1, 1, 1);
	if(f->my_mode==CRAZY1)
		glRotatef(MIN_FIN_ANGLE, 0.0, 1.0, 0.0);
	else
		glRotatef(shark->curr_fin_angle, 0.0, 1.0, 0.0);
	glCallList(shark->fins[shark->curr_fin]);
	glPopMatrix();
}

void draw_back_fins(FACE f){
	struct shark_struct* shark=(struct shark_struct*)f->char_struct;
	glPushMatrix();
	glTranslatef(0, fins, TOP_FIN_Y);
	glRotatef(180, 0.0, 1.0, 0.0);
	glScalef(.5*TOP_FIN_SCALE,TOP_FIN_SCALE,TOP_FIN_SCALE);
	glmSetMat(f->materials, f->mat_indeces[APPENDAGE]);
	glCallList(shark->fins[2]);
	glPopMatrix();

}

void draw_shark(FACE f, GLfloat angle, GLfloat yangle, BOOL left_open, BOOL right_open, GLfloat open, DIRECTION dir, OUTPUT_MODE mode){
	int next_face;
	struct shark_struct* shark;

	f->crazy_count--;
	if(f->crazy_count==0){
		f->my_mode = mode;
		if(mode!=NORMAL)
			f->crazy_count = CRAZY_COUNT;
		else
			f->crazy_count = 1;
	}
	apply_output_mode(f, &angle, &yangle, &left_open, &right_open, &open, &dir);
	next_face = NUM_SHARKS - open*NUM_SHARKS - 1;
	shark  = (struct shark_struct*)f->char_struct;
	if(next_face > shark->curr_face)
		shark->curr_face++;
	else if(next_face < shark->curr_face)
		shark->curr_face--;

	glPushMatrix();
	glRotatef(-90, 1.0, 0.0, 0.0);
	glRotatef(-yangle, 0.0, 0.0, -1.0);
	glRotatef(-angle, 0, 1, 0);
	shark_eyes(f, angle, yangle, left_open, right_open, dir);
	shark_fins(f, dir);
	//draw_back_fins(f);
	glmSetMat(f->materials, f->mat_indeces[HEAD]);
	glCallList(shark->faces[shark->curr_face]);
	glPopMatrix();
}


void init_shark(FACE f){
	int i;
	struct shark_struct* shark;
	f->char_struct = (struct shark_struct*)malloc(sizeof(struct shark_struct));
	f->materials = glmMTL(shark_mtl_file);
	f->mat_indeces=(GLint*)malloc(sizeof(GLint)*NUM_PARTS);
	//initialize all of the parts to some colors
	change_materials(f, init_shark_mats, NUM_PARTS);
	f->my_mode = NORMAL;
	f->eye_count = EYE_TIME*NUM_SHARK_LIDS-1;
	f->crazy_count = 1;
	f->curr_z_angle = 0;
	f->curr_eye_pop = 0;
	f->name = strdup("sharky");
	f->draw_func = draw_shark;
	shark = (struct shark_struct*)f->char_struct;

	printf("\nReading models: ");
	fflush(0);

	//initialize the draw lists
	init_lists(&shark->faces, &shark->m_faces, NUM_SHARKS, f->name, SHARK_HEAD_SCALE);
	init_lists(&shark->lids, &shark->m_lids, NUM_SHARK_LIDS, "sharkylid", SHARK_LID_SCALE);
	init_lists(&shark->fins, &shark->m_fins, NUM_FINS, "sharkyfin", SHARK_FIN_SCALE);
	init_lists(&shark->eyes, &shark->m_eyes, NUM_SHARK_EYES, "sharkyeye", SHARK_EYE_SCALE);
	init_lists(&shark->pupil, &shark->m_pupil, NUM_SHARK_PUPILS, "sharkypupil", SHARK_PUP_SCALE);
	init_lists(&shark->iris, &shark->m_iris, NUM_SHARK_IRIS, "sharkyiris", SHARK_IRIS_SCALE);

	printf("\n");
	fflush(0);

	shark->curr_face = 0;
	shark->curr_fin = 1;
	shark->curr_left_lid = 0;
	shark->curr_right_lid = 0;
	shark->curr_left_eye = 0;
	shark->curr_right_eye = 0;
	shark->curr_pupil = 0;
	shark->curr_fin_angle = 0;
}
