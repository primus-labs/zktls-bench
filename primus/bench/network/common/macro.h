#pragma once

// do not output any content if defined NO_PRINT
// you can pass -DOPT_NO_PRINT=ON while using cmake
#ifndef NO_PRINT
#define _printf(...) printf(__VA_ARGS__)
#define _cout(...) std::cout << __VA_ARGS__ << std::endl
#else
#define _printf(...)
#define _cout(...)
#endif
