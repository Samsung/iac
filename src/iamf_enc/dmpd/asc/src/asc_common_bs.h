#ifndef ASC_COMMON_BS_H
#define ASC_COMMON_BS_H

void audio_resizing_1ch(float * data0, int frame_size, int data_unit_size, int ds_factor, float** output, int* size);
void audio_resizing_714ch(float* data0, int frame_size, int data_unit_size, int ds_factor, float** output, int* size);

int get_decision_part(float prob_d, float prob_e, int d, int e, float thd, float the);

#endif 