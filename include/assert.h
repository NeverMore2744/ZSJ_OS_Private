#ifndef _ASSERT_H
#define _ASSERT_H

#include <driver/vga.h>

#undef assert
void assert(int statement, char * message);

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)                                \
    switch (x) { case 0: case (x): ; }

#endif // ! _ASSERT_H