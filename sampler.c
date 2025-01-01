#include "sampler.h"
#include <stdlib.h>

int init_sampler(struct sampler* s)
{
	if (s)
		free(s);

	s = malloc(sizeof(struct sampler));
	if (!s)
		return 1;

	s->bank = NULL;
	s->bank_size = 0;

	return 0;
}

int add_sample(struct sampler* s)
{
	struct sample* new_samp = malloc(sizeof(struct sample));
	if (!new_samp)
		return 1;

	new_samp->size = DEFAULT_SAMPLE_SIZE;

	new_samp->left = calloc(DEFAULT_SAMPLE_SIZE, sizeof(float));
	new_samp->right = calloc(DEFAULT_SAMPLE_SIZE, sizeof(float));
	if(!new_samp->left || !new_samp->right)
		return 1;

	return 0;
}
