#include <string>
using namespace std;

#define USE_WEBSOCKET_IO 0 

#if USE_WEBSOCKET_IO
#include "websocket_io_channel.h"
using PrimusIO = WebSocketIO;

inline PrimusIO* createPrimusIO(bool isAlice, const string& ip, int port) {
    PrimusIO* io = nullptr;
    if (isAlice) {
        io = new WebSocketIO(("ws://" + ip + ":" + std::to_string(port)).c_str());
    }
    else {
        io = new WebSocketIO(port);
    }
    io->Init();
    return io;
}
#else
#include <emp-tool/io/net_io_channel.h>
using namespace emp;
using PrimusIO = NetIO;

inline PrimusIO* createPrimusIO(bool isAlice, const string& ip, int port) {
    NetIO* io = new NetIO(isAlice ? ip.c_str() : nullptr, port);
    return io;
}
#endif

string test_protocol(const string& args);
string test_prot_on_off(const string& args);
string test_prove_proxy_tls(const string& args);
