#include "glm.h"

struct doggy_struct{
	GLint* faces, *lids, *ears, *eyes, *iris, *pupil;
	GLMmodel** m_faces, **m_lids, **m_ears, **m_eyes, **m_iris, **m_pupil;
	int curr_face, curr_ear, curr_left_lid, curr_right_lid, curr_right_eye, curr_left_eye, curr_pupil, eye_count;
	float curr_ear_angle;
};

void init_dog(FACE f);