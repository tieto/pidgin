#ifndef __FILTER_H__
#define __FILTER_H__

struct cc_features;

typedef struct filter_bank {
	float head_size[10];
	float head_z_rot[10];
	float head_y_rot[10];
	float mouth_open[10];
	float xfilt[10];
	float yfilt[10];
} filter_bank;	

filter_bank* Filter_Initialize (void);
void Filter_Destroy (filter_bank *f);
void filter(struct cc_features *instance, filter_bank *f);

#endif /* __FILTER_H__ */
