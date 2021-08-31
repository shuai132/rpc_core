#pragma once

#define __ASSERT(e, file, line) \
    ((void)printf ("%s:%d: failed assertion `%s'\n", file, line, e), abort())

#define ASSERT(e)  \
    ((void) ((e) ? ((void)0) : __ASSERT (#e, __FILE__, __LINE__)))
