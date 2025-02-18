#include "bench.h"
#include <thread>
#include <stdio.h>
#include <json.hpp>
using json = nlohmann::ordered_json;
#include "helper.h"

string result;
// bench dispatcher function
void do_main(const string& args) {
    printf("do_main:%s\n", args.c_str());
    result = "";
    json j = json::parse(args);
    string program = j["program"];
    if (program == "test_protocol") {
        // bench mpc model
        result = test_protocol(args);
    } else if (program == "test_prot_on_off") {
        // bench mpc model online-offline
        result = test_prot_on_off(args);
    } else if (program == "test_prove_proxy_tls") {
        // bench proxy model
        result = test_prove_proxy_tls(args);
    }
    printf("DONE:%s\n", result.c_str());
}

// entrypoint function for wasm bench function
PORT_FUNCTION(const char*) _main(const char* args) {
    std::thread t(do_main, string(args));
    t.detach();
    return result.c_str();
}

// entrypoint function for cpp bench function
int main(int argc, char** argv) {
#ifndef __EMSCRIPTEN__
    if (argc < 7) {
        printf("usage: %s $program $party $ip $port $request_size $response_size\n", argv[0]);
        exit(1);
    }

    json j = {{"program", argv[1]}, {"party", argv[2]},       {"ip", argv[3]},
              {"port", argv[4]},    {"requestSize", argv[5]}, {"responseSize", argv[6]}};
    do_main(j.dump());
#endif
    return 0;
}

// number of threads
int threads = 4;
// http request size
size_t QUERY_BYTE_LEN = 2 * 1024;
// http response size
size_t RESPONSE_BYTE_LEN = 2 * 1024;
