#include "charmony.h"

#ifdef CHY_HAS_STDBOOL_H
# include <stdbool.h>
#elif !defined(__cplusplus)
typedef char bool;
# define true 1
# define false 0
#endif

#define CMARK_ATTRIBUTE(list)

#ifndef CHY_HAS_VA_COPY
  #define va_copy(dest, src) ((dest) = (src))
#endif
