#ifndef SP_PLUS_ASSERT_H
#define SP_PLUS_ASSERT_H

#include <signal.h>

#ifdef DEBUG 
#define ASSERT(x) if (!(x)) raise(SIGTERM);
#else
#define ASSERT(x)
#endif

#endif
