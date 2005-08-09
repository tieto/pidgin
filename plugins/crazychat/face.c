#include "face.h"
#include "sharky.h"
#include "doggy.h"
#include <assert.h>

FACE init_face(KIND kind)
{
	FACE face;
	face = (FACE)malloc(sizeof(*face));

	switch(kind){
		case DOG:
			init_dog(face);			
			break;
		case SHARK:
			init_shark(face);
			break;
		default:
			printf("default face\n");
			init_dog(face);
			break;
	}			
	return face;	
}

void draw_face(FACE face, GLfloat zrot, GLfloat yrot, BOOL left_eye, BOOL right_eye, GLfloat mouth_open, DIRECTION dir, OUTPUT_MODE mode){
	face->draw_func(face, zrot, yrot, left_eye, right_eye, mouth_open, dir, mode);
}

void change_materials(FACE f, int* mats, int num_change){
	int i;
	assert(!(num_change<0 || num_change>NUM_PARTS));
	for(i=0;i<num_change;i++){
		f->mat_indeces[i]=mats[i];
	}
}

void free_face(FACE f){}
