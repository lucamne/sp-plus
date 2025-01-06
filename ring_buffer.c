#include "ring_buffer.h"

#include <stdlib.h>
#include <string.h>

struct ring_buf* init_ring_buf(int size)
{
	struct ring_buf* rb = malloc(sizeof(struct ring_buf));
	if (!rb)
		return NULL;

	rb->data = calloc(1, size);
	rb->data_size = size;
	rb->head = 0;
	rb->in_use = 0;
	return rb;
}

int write_to_buf(struct ring_buf* rb, const void* src, int size)
{
	// not enough space in buffer
	if (size > rb->data_size - rb->in_use)
		return 0;

	// number of bytes before wrap is needed
	const int n = rb->data_size - rb->head; 
	// wrap if needed
	if (n >= size) {
		memcpy(rb->data + rb->head, src, size);
	} else if (n < size) {
		memcpy(rb->data + rb->head, src, n);
		memcpy(rb->data, (char *) src + n, size - n);
	}

	rb->head = (rb->head + size) % rb->data_size;
	rb->in_use += size;
	return size;
}

int read_to_dest(struct ring_buf* rb, void* dest, int size)
{
	if (size > rb->in_use)
		return 0;

	const int read = (rb->head + (rb->data_size - rb->in_use)) % rb->data_size;
	// number of bytes before wrap is needed
	const int n = rb->data_size - read;
	// wrap if needed
	if (n >= size) {
		memcpy(dest, rb->data + read, size);
	} else if (n < size) {
		memcpy(dest, rb->data + read, n);
		memcpy((char *) dest + n, rb->data, size - n);
	}

	rb->in_use -= size;
	return size;
}
