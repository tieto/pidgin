#include "face.h"
#include "doggy.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include "models.h"

#define NUM_DOGS 11
#define NUM_DOG_LIDS 10
#define NUM_EARS 3
#define NUM_EYES 1
#define NUM_PUPILS 1
#define NUM_IRIS 1
#define SCALE .020
#define EYES_Y 32.78*SCALE // .295
#define EYES_X 28.89*SCALE // .26
#define EYES_Z 42.22*SCALE // .38
#define EARS_X 60*SCALE //.65
#define EARS_Y 20*SCALE//.18
#define EARS_Z -5.56*SCALE //.05
#define IRIS_X 0*SCALE
#define IRIS_Y 1.67*SCALE//.015
#define IRIS_Z 7*SCALE//.08
#define PUP_X 0*SCALE
#define PUP_Y 0*SCALE 
#define PUP_Z 1.2*SCALE //.028
#define IRIS_SCALE .12*SCALE
#define PUP_SCALE .11*SCALE
#define EAR_SCALE .7*SCALE
#define EYE_SCALE .7*SCALE
#define LID_SCALE .77*SCALE
#define DOG_SCALE .58*SCALE
#define MAX_FILE_LEN 64
#define MAX_EAR_ANGLE 90.0
#define MIN_EAR_ANGLE -20.0


char dog_mtl_file[MAX_FILE_LEN] = "dog.mtl";
// the initial dog materials
GLint init_dog_mats[NUM_PARTS] = {1, 2, 2, 4, 0, 3, 5, 0, 0, 0};

void draw_pupil(FACE f, PART p) {
	struct doggy_struct* dog=(struct doggy_struct*)f->char_struct;
	glPushMatrix();
	glTranslatef(IRIS_X, -IRIS_Z, IRIS_Y);
	if(p==LEFT_IRIS)
		glmSetMat(f->materials, f->mat_indeces[LEFT_IRIS]);
	else
		glmSetMat(f->materials, f->mat_indeces[RIGHT_IRIS]);
	glCallList(dog->iris[0]);
	glTranslatef(PUP_X, -PUP_Z, PUP_Y);
	glmSetMat(f->materials, f->mat_indeces[PUPIL]);
	glCallList(dog->pupil[0]);
	glPopMatrix();
}

void draw_left_eye(FACE f, BOOL open, int max) {
	struct doggy_struct* dog=(struct doggy_struct*)f->char_struct;
	if(f->my_mode==CRAZY2)
		dog->curr_left_lid=NUM_DOG_LIDS-1;
	else
		dog->curr_left_lid = compute_lid(open, dog->curr_left_lid, max);
	glPushMatrix();
	glTranslatef(-EYES_X, 0.0, 0.0);
	glPushMatrix();
	glTranslatef(0.0, -f->curr_eye_pop, 0.0);
	draw_pupil(f, LEFT_IRIS);
	glmSetMat(f->materials, f->mat_indeces[EYES]);
	glCallList(dog->eyes[dog->curr_left_eye]);
	glPopMatrix();
	glmSetMat(f->materials, f->mat_indeces[LIDS]);
	glCallList(dog->lids[dog->curr_left_lid]);
	glPopMatrix();
}

void draw_right_eye(FACE f, BOOL open, int max) {
	struct doggy_struct* dog=(struct doggy_struct*)f->char_struct;
	if(f->my_mode==CRAZY2)
		dog->curr_right_lid=NUM_DOG_LIDS-1;
	else
		dog->curr_right_lid = compute_lid(open, dog->curr_right_lid, max);
	glPushMatrix();
	glTranslatef(EYES_X, 0.0, 0.0);
		glScalef(-1, 1, 1);
	glPushMatrix();
	glTranslatef(0.0, -f->curr_eye_pop, 0.0);
	draw_pupil(f, RIGHT_IRIS);
	glmSetMat(f->materials, f->mat_indeces[EYES]);
	glCallList(dog->eyes[dog->curr_right_eye]);
	glPopMatrix();
	glmSetMat(f->materials, f->mat_indeces[LIDS]);
	glCallList(dog->lids[dog->curr_right_lid]);
	glPopMatrix();
}

void dog_eyes(FACE f, GLfloat angle, GLfloat yangle, BOOL left_open, BOOL right_open, DIRECTION dir)
{
	struct doggy_struct* dog=(struct doggy_struct*)f->char_struct;
	int max_eye;
	if(dir==CONST) { //then not moving, eyes are gettin sleepy
		f->eye_count--;
	}
	else{
		f->eye_count=EYE_TIME*NUM_DOG_LIDS-1;
	}
	max_eye=f->eye_count/EYE_TIME;
	if(max_eye<0)
		max_eye=0;
	if(f->my_mode==CRAZY2)
		f->curr_eye_pop=f->curr_eye_pop + (MAX_EYE_POP - f->curr_eye_pop)/2;
	else
		f->curr_eye_pop=f->curr_eye_pop - (f->curr_eye_pop-0)/2;
	glPushMatrix();
	glTranslatef(0, 0, EYES_Y);
	glTranslatef(0, -EYES_Z,0);
	draw_left_eye(f, left_open, max_eye);
	draw_right_eye(f, right_open, max_eye);	
	glPopMatrix();
}

void dog_ears(FACE f, DIRECTION dir){
	struct doggy_struct* dog=(struct doggy_struct*)f->char_struct;
	//printf("ears %f\n", ears);
	if(dir==DOWN){
		if(dog->curr_ear < (NUM_EARS-1))
			dog->curr_ear++;
		dog->curr_ear_angle = dog->curr_ear_angle+(MAX_EAR_ANGLE-dog->curr_ear_angle)/2;
	}
	if(dir==UP){
		if(dog->curr_ear > 0)
			dog->curr_ear--;
		dog->curr_ear_angle = dog->curr_ear_angle+(MIN_EAR_ANGLE-dog->curr_ear_angle)/2;
	}
	else if(dir==CONST){
		dog->curr_ear=1;
		dog->curr_ear_angle = dog->curr_ear_angle+(0-dog->curr_ear_angle)/3;
	}

	glPushMatrix();
	glTranslatef(-EARS_X, -EARS_Z, EARS_Y);
	if(f->my_mode==CRAZY1)
		glRotatef(MAX_EAR_ANGLE, 0.0, 1.0, 0.0);
	else
		glRotatef(dog->curr_ear_angle, 0.0, 1.0, 0.0);
	glmSetMat(f->materials, f->mat_indeces[APPENDAGE]);
	glCallList(dog->ears[dog->curr_ear]);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(EARS_X, -EARS_Z, EARS_Y);
	glScalef(-1, 1, 1);
	if(f->my_mode==CRAZY1)
		glRotatef(MIN_EAR_ANGLE, 0.0, 1.0, 0.0);
	else
		glRotatef(dog->curr_ear_angle, 0.0, 1.0, 0.0);
	glCallList(dog->ears[dog->curr_ear]);
	glPopMatrix();
}

void draw_dog(FACE f, GLfloat angle, GLfloat yangle, BOOL left_open, BOOL right_open, GLfloat open, DIRECTION dir, OUTPUT_MODE mode){
	int next_face; 
	struct doggy_struct* dog;	
	f->crazy_count--;
	if(f->crazy_count==0){
		f->my_mode = mode;
		if(mode!=NORMAL)
			f->crazy_count = CRAZY_COUNT;
		else
			f->crazy_count = 1;
	}
	apply_output_mode(f, &angle, &yangle, &left_open, &right_open, &open, &dir);
	next_face = NUM_DOGS - open*NUM_DOGS - 1;
	dog  = (struct doggy_struct*)f->char_struct;
	if(next_face > dog->curr_face)
		dog->curr_face++;
	else if(next_face < dog->curr_face)
		dog->curr_face--;

	glPushMatrix();
	glRotatef(-90, 1.0, 0.0, 0.0);
	glRotatef(-yangle, 0.0, 0.0, -1.0);
	glRotatef(-angle, 0, 1, 0);
	dog_eyes(f, angle, yangle, left_open, right_open, dir);	
	dog_ears(f, dir);
	glmSetMat(f->materials, f->mat_indeces[HEAD]);
	glCallList(dog->faces[dog->curr_face]);
	glPopMatrix();
}

void init_dog(FACE f){
	int i;
	struct doggy_struct* dog;
	f->char_struct = (struct doggy_struct*)malloc(sizeof(struct doggy_struct));
	f->materials = glmMTL(dog_mtl_file);
	f->mat_indeces=(GLint*)malloc(sizeof(GLint)*NUM_PARTS);
	//initialize all of the parts to some colors
	change_materials(f, init_dog_mats, NUM_PARTS);
	f->my_mode = NORMAL;
	f->eye_count = EYE_TIME*NUM_DOG_LIDS-1;
	f->crazy_count = 1;
	f->curr_z_angle = 0;
	f->curr_eye_pop = 0;
	f->name = strdup("dog");
	f->draw_func = draw_dog;
	dog = (struct doggy_struct*)f->char_struct;
	
	printf("\nReading models: ");
	fflush(0);
	
	//initialize the draw lists
	init_lists(&dog->faces, &dog->m_faces, NUM_DOGS, f->name, DOG_SCALE);
	init_lists(&dog->lids, &dog->m_lids, NUM_DOG_LIDS, "lid", LID_SCALE);
	init_lists(&dog->ears, &dog->m_ears, NUM_EARS, "ear", EAR_SCALE);
	init_lists(&dog->eyes, &dog->m_eyes, NUM_EYES, "dogeye", EYE_SCALE);
	init_lists(&dog->pupil, &dog->m_pupil, NUM_PUPILS, "dogpupil", PUP_SCALE);
	init_lists(&dog->iris, &dog->m_iris, NUM_IRIS, "dogiris", IRIS_SCALE);

	printf("\n");
	fflush(0);
		
	dog->curr_face = 0;
	dog->curr_ear = 1;
	dog->curr_left_lid = 9;
	dog->curr_right_lid = 0;
	dog->curr_left_eye = 0;
	dog->curr_right_eye = 0;
	dog->curr_pupil = 0;
	dog->curr_ear_angle = 0;
}
