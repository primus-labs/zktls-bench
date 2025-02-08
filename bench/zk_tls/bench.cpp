#include "bench.h"
#include <thread>
#include <stdio.h>
#include <json.hpp>
using json = nlohmann::ordered_json;
#include "helper.h"

string result;
void do_main(const string& args) {
	printf("do_main:%s\n", args.c_str());
	result = "";
	json j = json::parse(args);
	string program = j["program"];
	if (program == "test_protocol") {
	    result = test_protocol(args);
	}
	else if (program == "test_prot_on_off") {
		result = test_prot_on_off(args);
	}
	else if (program == "test_prove_proxy_tls") {
		result = test_prove_proxy_tls(args);
	}
}

PORT_FUNCTION(const char*) _main(const char* args) {
    std::thread t(do_main, string(args));
    t.detach();
    return result.c_str();
}

int main(int argc, char** argv) {
#ifndef __EMSCRIPTEN__
    if (argc < 9) {
        printf("usage: %s $program $party $ip $port $request_size $response_size $bandwith $latency\n", argv[0]);
        exit(1);
    }

    json j = {
		{"program", argv[1]},
        {"party", argv[2]},
        {"ip", argv[3]},
        {"port", argv[4]},
        {"requestSize", argv[5]},
        {"responseSize", argv[6]}
    };
    do_main(j.dump());
    const char* s = result.c_str();
    printf("result:%s\n", s);

    json j2 = json::parse(s);
    int requestSize = j2["requestSize"];
    int responseSize = j2["responseSize"];
    double sendBytes = j2["sendBytes"];
    double totalCost = j2["totalCost"];

    char filename[256];
    sprintf(filename, "output_%s_%s.csv", argv[1], argv[2]); 
    FILE* fp = fopen(filename, "a");
    fprintf(fp, "%s,%s,%d,%d,%.3f,%.3f\n", argv[7], argv[8], requestSize, responseSize, sendBytes, totalCost);
    fclose(fp);
#endif
    return 0;
}

