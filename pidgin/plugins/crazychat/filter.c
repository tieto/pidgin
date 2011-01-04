#include "cc_interface.h"
#include "filter.h"
#include "stdio.h"

#define coef_0 .0022
#define coef_1 .0174
#define coef_2 .0737
#define coef_3 .1662
#define coef_4 .2405

filter_bank* Filter_Initialize (void)
{
	filter_bank* new_filter;
	new_filter=(filter_bank*)malloc(sizeof(filter_bank));
	memset(new_filter,0,sizeof(filter_bank));
//	fprintf(stderr,"RESETTING FILTER BANK *******************\n");
	return new_filter;
}

void Filter_Destroy (filter_bank *f)
{
	free(f);
}

void filter(struct cc_features *features, filter_bank *f)
{

f->head_size[9]=f->head_size[8];
f->head_size[8]=f->head_size[7];
f->head_size[7]=f->head_size[6];
f->head_size[6]=f->head_size[5];
f->head_size[5]=f->head_size[4];
f->head_size[4]=f->head_size[3];
f->head_size[3]=f->head_size[2];
f->head_size[2]=f->head_size[1];
f->head_size[1]=f->head_size[0];
f->head_size[0]=features->head_size;

features->head_size=(guint8) ( (coef_0*(f->head_size[0]+f->head_size[9]))+(coef_1*(f->head_size[1]+f->head_size[8])) +
			(coef_2*(f->head_size[2]+f->head_size[7])) + (coef_3*(f->head_size[3]+f->head_size[6]))
				+ (coef_4*(f->head_size[4]+f->head_size[5])));



f->head_z_rot[9]=f->head_z_rot[8];
f->head_z_rot[8]=f->head_z_rot[7];
f->head_z_rot[7]=f->head_z_rot[6];
f->head_z_rot[6]=f->head_z_rot[5];
f->head_z_rot[5]=f->head_z_rot[4];
f->head_z_rot[4]=f->head_z_rot[3];
f->head_z_rot[3]=f->head_z_rot[2];
f->head_z_rot[2]=f->head_z_rot[1];
f->head_z_rot[1]=f->head_z_rot[0];
f->head_z_rot[0]=features->head_z_rot;

features->head_z_rot=(guint8) ( (coef_0*(f->head_z_rot[0]+f->head_z_rot[9]))+(coef_1*(f->head_z_rot[1]+f->head_z_rot[8])) +
			(coef_2*(f->head_z_rot[2]+f->head_z_rot[7])) + (coef_3*(f->head_z_rot[3]+f->head_z_rot[6]))
				+ (coef_4*(f->head_z_rot[4]+f->head_z_rot[5])));


f->head_y_rot[9]=f->head_y_rot[8];
f->head_y_rot[8]=f->head_y_rot[7];
f->head_y_rot[7]=f->head_y_rot[6];
f->head_y_rot[6]=f->head_y_rot[5];
f->head_y_rot[5]=f->head_y_rot[4];
f->head_y_rot[4]=f->head_y_rot[3];
f->head_y_rot[3]=f->head_y_rot[2];
f->head_y_rot[2]=f->head_y_rot[1];
f->head_y_rot[1]=f->head_y_rot[0];
f->head_y_rot[0]=features->head_y_rot;

features->head_y_rot=(guint8) ( (coef_0*(f->head_y_rot[0]+f->head_y_rot[9]))+(coef_1*(f->head_y_rot[1]+f->head_y_rot[8])) +
			(coef_2*(f->head_y_rot[2]+f->head_y_rot[7])) + (coef_3*(f->head_y_rot[3]+f->head_y_rot[6]))
				+ (coef_4*(f->head_y_rot[4]+f->head_y_rot[5])));


f->xfilt[9]=f->xfilt[8];
f->xfilt[8]=f->xfilt[7];
f->xfilt[7]=f->xfilt[6];
f->xfilt[6]=f->xfilt[5];
f->xfilt[5]=f->xfilt[4];
f->xfilt[4]=f->xfilt[3];
f->xfilt[3]=f->xfilt[2];
f->xfilt[2]=f->xfilt[1];
f->xfilt[1]=f->xfilt[0];
f->xfilt[0]=features->x;

features->x=(guint8) ( (coef_0*(f->xfilt[0]+f->xfilt[9]))+(coef_1*(f->xfilt[1]+f->xfilt[8])) +
			(coef_2*(f->xfilt[2]+f->xfilt[7])) + (coef_3*(f->xfilt[3]+f->xfilt[6]))
				+ (coef_4*(f->xfilt[4]+f->xfilt[5])));


f->yfilt[9]=f->yfilt[8];
f->yfilt[8]=f->yfilt[7];
f->yfilt[7]=f->yfilt[6];
f->yfilt[6]=f->yfilt[5];
f->yfilt[5]=f->yfilt[4];
f->yfilt[4]=f->yfilt[3];
f->yfilt[3]=f->yfilt[2];
f->yfilt[2]=f->yfilt[1];
f->yfilt[1]=f->yfilt[0];
f->yfilt[0]=features->y;

features->y=(guint8) ( (coef_0*(f->yfilt[0]+f->yfilt[9]))+(coef_1*(f->yfilt[1]+f->yfilt[8])) +
			(coef_2*(f->yfilt[2]+f->yfilt[7])) + (coef_3*(f->yfilt[3]+f->yfilt[6]))
				+ (coef_4*(f->yfilt[4]+f->yfilt[5])));


f->mouth_open[9]=f->mouth_open[8];
f->mouth_open[8]=f->mouth_open[7];
f->mouth_open[7]=f->mouth_open[6];
f->mouth_open[6]=f->mouth_open[5];
f->mouth_open[5]=f->mouth_open[4];
f->mouth_open[4]=f->mouth_open[3];
f->mouth_open[3]=f->mouth_open[2];
f->mouth_open[2]=f->mouth_open[1];
f->mouth_open[1]=f->mouth_open[0];
f->mouth_open[0]=features->mouth_open;

features->mouth_open=(guint8) ( (coef_0*(f->mouth_open[0]+f->mouth_open[9]))+(coef_1*(f->mouth_open[1]+f->mouth_open[8])) +
			(coef_2*(f->mouth_open[2]+f->mouth_open[7])) + (coef_3*(f->mouth_open[3]+f->mouth_open[6]))
				+ (coef_4*(f->mouth_open[4]+f->mouth_open[5])));

}
