#ifndef TRANS_H
#define TRANS_H

struct trans {
	void (*free)(struct trans *);
	void (*slide)(struct trans *, float *, int);
	void (*get)(struct trans *, float *);
	void *data;
};

void slide_stft(struct trans *trans, float *in, int N);
void get_stft(struct trans *trans, float *out);
void free_stft(struct trans *trans);
struct trans *create_stft(int bins);

#endif

