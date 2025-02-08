#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define PORT_FUNCTION(ret_type) extern "C" ret_type EMSCRIPTEN_KEEPALIVE
#else
#define PORT_FUNCTION(ret_type) extern "C" ret_type
#endif

