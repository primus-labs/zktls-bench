#include <string>
using namespace std;

// 1: use websocket io
// 0: use net io
#define USE_WEBSOCKET_IO 1 

#if USE_WEBSOCKET_IO
#include "websocket_io_channel.h"
using PrimusIO = WebSocketIO;

// create websocket io
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

// create net io
inline PrimusIO* createPrimusIO(bool isAlice, const string& ip, int port) {
    NetIO* io = new NetIO(isAlice ? ip.c_str() : nullptr, port);
    return io;
}
#endif

// mpc model bench function
string test_protocol(const string& args);
// mpc model online-offline bench function
string test_prot_on_off(const string& args);
// proxy model bench function
string test_prove_proxy_tls(const string& args);
