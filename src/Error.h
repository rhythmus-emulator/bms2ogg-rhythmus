#include <assert.h>

#ifdef _DEBUG
# define ASSERT(x) assert(x)
# define DASSERT(x) assert(x)
#else
# define ASSERT(x) assert(x)
# define DASSERT(x)
#endif