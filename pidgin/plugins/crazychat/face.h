#ifndef __FACE_H__
#define __FACE_H__

#include <GL/gl.h>
#include <GL/glu.h>
#include "glm.h"

#define NUM_PARTS 10
#define DOG_SHARK_CHANGE 5
#define CRAZY_COUNT 12
#define MAX_EYE_POP 1.3
#define EYE_TIME 120
#define MAX_FILE_LEN 64
#define ANGLE_INC 60

typedef enum {DOG, SHARK} KIND;
typedef int BOOL;
typedef enum {UP, DOWN, CONST} DIRECTION;
typedef enum {NORMAL, CRAZY1, CRAZY2, CRAZY3} OUTPUT_MODE;

struct face_struct {
	char* name;
	KIND my_kind;
	void* char_struct;
	GLint* mat_indeces;
	GLMmat_str* materials;
	OUTPUT_MODE my_mode;
	int eye_count, crazy_count;
	void (*draw_func)(struct face_struct*, GLfloat, GLfloat, BOOL, BOOL, GLfloat, DIRECTION, OUTPUT_MODE);
	float curr_z_angle, curr_eye_pop;
};

typedef struct face_struct* FACE;
typedef enum {APPENDAGE, HEAD, LIDS, LEFT_IRIS, RIGHT_IRIS, EYES, PUPIL, EXTRA1, EXTRA2, EXTRA3} PART;

FACE init_face(KIND kind);

FACE copy_face(FACE f);

void draw_face(FACE face, GLfloat zrot, GLfloat yrot, BOOL left_eye, BOOL right_eye, GLfloat mouth_open, DIRECTION dir, OUTPUT_MODE mode);

void change_materials(FACE face, int* mats, int num_change);

#endif
