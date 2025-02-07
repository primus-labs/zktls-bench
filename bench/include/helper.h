#include <emscripten.h>

#ifdef __EMSCRIPTEN__
#define PORT_FUNCTION(ret_type) extern "C" ret_type EMSCRIPTEN_KEEPALIVE
#else
#define PORT_FUNCTION(ret_type) extern "C" ret_type
#endif

