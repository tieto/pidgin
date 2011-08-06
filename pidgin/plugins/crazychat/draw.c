#include "righteye10.h"

typedef struct _point{
  GLshort x;
  GLshort y;
} Point;

typedef struct Packet{
  Point mouthLeft;
  Point mouthRight;
  Point mouthTop;
  Point mouthBottom;
  Point eyeLeft;
  Point eyeRight;
} Packet;

#define NUM_EYE_FRAMES 10

GLint rightEyes[NUM_EYE_FRAMES];

void CalculateMouthPoints(GLshort[][][] ctrlpoints, struct Packet* p)
{
    GLshort points[4][3];
    points[0][0]=p->mouthLeft.x;
    points[0][1]=p->mouthLeft.y;
    points[0][2]=front;

    points[1][0]=p->mouthBottom.x;
    points[1][1]=p->mouthTop.y;
    points[1][2]=front;

    points[2][0]=p->mouthRight.x;
    points[2][1]=p->mouthRight.y;
    points[2][2]=front;

    points[3][0]=p->mouthTop.x;
    points[3][1]=p->mouthTop.y;
    points[3][2]=front;

    ctrlpoints[0][0][0]=points[0][0];
    ctrlpoints[0][0][1]=points[0][1];
    ctrlpoints[0][0][2]=points[0][2];

    ctrlpoints[0][1][0]=points[1][0];
    ctrlpoints[0][1][1]=points[1][1];
    ctrlpoints[0][1][2]=points[1][2];

    ctrlpoints[0][2][0]=points[2][0];
    ctrlpoints[0][2][1]=points[2][1];
    ctrlpoints[0][2][2]=points[2][2];

    ctrlpoints[1][0][0]=points[0][0];
    ctrlpoints[1][0][1]=points[0][1];
    ctrlpoints[1][0][2]=points[0][2];

    ctrlpoints[1][1][0]=0;
    ctrlpoints[1][1][1]=0;
    ctrlpoints[1][1][2]=back;

    ctrlpoints[1][2][0]=points[2][0];
    ctrlpoints[1][2][1]=points[2][1];
    ctrlpoints[1][2][2]=points[2][2];

    ctrlpoints[2][0][0]=points[0][0];
    ctrlpoints[2][0][1]=points[0][1];
    ctrlpoints[2][0][2]=points[0][2];

    ctrlpoints[2][1][0]=points[3][0];
    ctrlpoints[2][1][1]=points[3][1];
    ctrlpoints[2][1][2]=points[3][2];

    ctrlpoints[2][2][0]=points[2][0];
    ctrlpoints[2][2][1]=points[2][1];
    ctrlpoints[2][2][2]=points[2][2];
}

void drawMouth(struct Packet* p)
{
    GLshort[4][3][3] ctrlpoints;
    CalculateMouthPoints(ctrlpoints, p);

    glMap2f(GL_MAP2_VERTEX_3, 0, 10, 3, 3, 0.0, 10.0, 9, 3, &ctrlpoints[0][0][0]);
    glEnable(GL_MAP2_VERTEX_3);
    glMapGrid2f(10, 0, 10, 10, 0, 10);
    glEnable(GL_AUTO_NORMAL);
    glEvalMesh2(GL_FILL, 0, 10, 0, 10);
}

void initEyes(){


void drawEyes(struct Packet* p){
    GLshort eye[3][3][3];
    CalculateEyePoints(eye, p, LEFT);
    glMap2f(GL_MAP2_VERTEX_3, 0, 10, 3, 3, 0.0, 10.0, 9, 3, &eyep[0][0][0]);
    glMapGrid2f(10, 0, 10, 10, 0, 10);
    glEvalMesh2(GL_FILL, 0, 10, 0, 10);

    CalculateEyePoints(eye, p, RIGHT);
    glMap2f(GL_MAP2_VERTEX_3, 0, 10, 3, 3, 0.0, 10.0, 9, 3, &eyep[0][0][0]);
    glMapGrid2f(10, 0, 10, 10, 0, 10);
    glEvalMesh2(GL_FILL, 0, 10, 0, 10);

}

void drawHead(struct Packet* p){
}
