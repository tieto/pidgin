#include"glm.h"
#include <GL/gl.h>
#include <GL/glu.h>

void init_lists(GLint** lists, GLMmodel*** models, int num_lists, char* name, float my_scale);

int compute_lid(BOOL open, int curr_lid, int max);

void apply_output_mode(FACE f, GLfloat* angle, GLfloat* yangle, BOOL* left_open, BOOL* right_open, GLfloat* open, DIRECTION* dir);