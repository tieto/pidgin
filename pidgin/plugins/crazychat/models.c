#include"glm.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include "face.h"

#define ASCII_DIGITS 48

void init_lists(GLint** lists, GLMmodel*** models, int num_lists, char* name, float my_scale){
	int i;
	char c;
	GLint* temp_lists;
	GLMmodel** temp_models;
	char* temp = (char*)malloc(sizeof(char) * (strlen(name) + strlen(".obj" + 3)));
	GLMmodel* model;
	float dum;
	temp_lists = (GLint*)malloc(sizeof(GLint) * num_lists);
	temp_models = (GLMmodel**)malloc(sizeof(GLMmodel*) * num_lists);

	for(i=0;i<num_lists;i++) {
		strcpy(temp, name);
		if(i<10){
			c = (char)(ASCII_DIGITS+i);
			strncat(temp, &c, 1);
		}
		else{
			c = (char)(i/10+ASCII_DIGITS);
			strncat(temp, &c, 1);
			c = (char)(i%10 + ASCII_DIGITS);
			strncat(temp, &c, 1);
		}
		strcat(temp, ".obj");
		temp_models[i]=glmReadOBJ(temp);
		glmScale(temp_models[i], my_scale);
		dum =glmUnitize(temp_models[i]); // this actually just centers
		//printf("%s factor %f", temp, dum);
		temp_lists[i]=glmList(temp_models[i], GLM_SMOOTH);
	}

	*lists = temp_lists;
	*models = temp_models;
	free(temp);
}

int compute_lid(BOOL open, int curr_lid, int max){
	if(open) {
		if(curr_lid < max){
			curr_lid++;
		}
		else
			curr_lid=max;
	}
	else {
		if(curr_lid >=2){
			curr_lid-=2;
		}
		else if(curr_lid==1){
			curr_lid--;
		}
		curr_lid=0;
	}
	return curr_lid;
}

void apply_output_mode(FACE f, GLfloat* angle, GLfloat* yangle, BOOL* left_open, BOOL* right_open, GLfloat* open, DIRECTION* dir)
{
	struct doggy_struct* dog = (struct doggy_struct*)f->char_struct;
	if(f->my_mode==NORMAL){
		f->curr_z_angle=0;
		return;
	}
	if(f->my_mode==CRAZY1){
		f->curr_z_angle = f->curr_z_angle+ANGLE_INC;
		*angle = f->curr_z_angle;
	}
}
