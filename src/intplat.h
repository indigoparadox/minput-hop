
#ifndef INTPLAT_H
#define INTPLAT_H

#ifdef MINPUT_OS_WIN32

typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef long int32_t;
typedef int ssize_t;
typedef signed char int8_t;

#elif defined MINPUT_OS_WIN16

typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef long int32_t;
typedef int ssize_t;
typedef unsigned int size_t;
typedef signed char int8_t;

#else
#  include <stdint.h>
#endif

#endif /* !INTPLAT_H */

