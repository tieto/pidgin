#include "glm.h"

struct shark_struct{
	GLint* faces, *lids, *fins, *eyes, *iris, *pupil;
	GLMmodel** m_faces, **m_lids, **m_fins, **m_eyes, **m_iris, **m_pupil;
	int curr_face, curr_fin, curr_left_lid, curr_right_lid, curr_right_eye, curr_left_eye, curr_pupil, eye_count;
	float curr_fin_angle;
};

void init_shark(FACE f);