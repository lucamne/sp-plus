#include "sampler.h"

#include <stdlib.h>
#include <assert.h>


struct sampler* init_sampler(void)
{
	struct sampler* s = malloc(sizeof(struct sampler));
	assert(s);

	s->bank = NULL;
	s->bank_size = 0;

	return s;
}


