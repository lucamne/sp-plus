#ifndef RING_BUFFER_H
#define RING_BUFFER_H

struct ring_buf {
	char* data;
	int data_size;		// size of buffer in bytes
	int head;		// index of write head
	int in_use;		// bytes in use
};

// initialize ring buffer with 'size' bytes
struct ring_buf* init_ring_buf(int size);

// returns bytes written
int write_to_buf(struct ring_buf* rb, const void* src, int size);

// returns bytes read to dest
int read_to_dest(struct ring_buf* rb, void* dest, int size);

#endif
