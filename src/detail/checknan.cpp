#pragma once

#ifdef NDEBUG
#define nancheck(arr, name) ((void)0)
#else
#define nancheck(arr, name) {if(!std::isfinite(sum(arr))) {\
                               std::cout << "A not-finite number detected in: " << name << std::endl;\
                               std::cout << arr;\
                               assert(0);}}
#endif

#ifdef NDEBUG
#define negcheck(arr, name) ((void)0)
#else
#define negcheck(arr, name) {if(min(arr) < 0.) {\
                               std::cout << "A negative number detected in: " << name << std::endl;\
                               std::cout << arr;\
                               assert(0);}}
#endif

// actually not to zero, but to 1e-10 (we need rv>0 in libcloud and cond substepping numerical errors colud lead to rv<0 if we would set it here to 0)
#define negtozero(arr, name) {if(min(arr) < 0.) {\
                               std::cout << "A negative number detected in: " << name << std::endl;\
                               std::cout << arr;\
                               std::cout << "CHEATING: turning negative values to zeroes" << std::endl;\
                               arr = where(arr <= 0., 1e-10, arr);\
                               }}
