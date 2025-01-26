#include "system.h"
#include "defs.h"

#include <string.h>
#include <stdlib.h>

// attach child to the input of parent
int add_bus_in(struct bus* parent, struct bus* child)
{
	parent->bus_ins = realloc(
			parent->bus_ins, 
			++(parent->num_bus_ins) * sizeof(struct bus*));
	if (!parent->bus_ins)
		return 1;

	parent->bus_ins[parent->num_bus_ins - 1] = child;
	return 0;
}

// set bus attenuation
int set_atten(struct bus* b, float a)
{
	b->atten = a;
	if (b->atten < 0.0f)
		b->atten = 0.0f;
	else if (b->atten > 1.0f)
		b->atten = 1.0f;
	return 0;
}

// set bus pan 
int set_pan(struct bus* b, float p)
{
	b->pan = p;
	if (b->pan < -1.0f)
		b->pan = -1.0f;
	else if (b->pan > 1.0f)
		b->pan = 1.0f;
	return 0;
}

