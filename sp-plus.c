#include "sampler.h"
#include <stdio.h>

int main(int argc, char** argv)
{
	struct sampler* s = NULL; 
	if (init_sampler(s)) {
		fprintf(stderr, "Error initializing sampler\n");
		return 0;
	}

	if (add_sample(s)) {
		fprintf(stderr, "Error adding sample\n");
		return 0;
	}

	return 0;
}
