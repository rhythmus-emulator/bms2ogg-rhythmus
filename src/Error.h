#include <assert.h>

#ifndef ASSERT

#ifdef _DEBUG
# define ASSERT(x) assert(x)
# define DASSERT(x) assert(x)
#else
# define ASSERT(x) assert(x)
# define DASSERT(x)
#endif

#endif