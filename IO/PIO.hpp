#ifndef included_PIO_hpp
#define included_PIO_hpp

#include "IO/PIO.h"

#include <iostream>
#include <stdarg.h>
#include <cstdio>


namespace IO {


inline int printp( const char *format, ... )
{
    va_list ap; 
    va_start(ap,format); 
    char tmp[1024];
    int n = vsprintf(tmp,format,ap);
    va_end(ap);
    pout << tmp;
    pout.flush();
    return n;
}


} // IO namespace

#endif
