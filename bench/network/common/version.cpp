#include "version.h"
#include <regex>
#include <iostream>
using namespace std;
#include "common/macro.h"


int32_t pado_version_number() {
    return PADO_MAJOR_VERSION * 1000 * 1000 + PADO_MINOR_VERSION * 1000 + PADO_BUILD_VERSION;
}
string pado_version_string() {
    return to_string(PADO_MAJOR_VERSION) + string(".") + to_string(PADO_MINOR_VERSION) +
           string(".") + to_string(PADO_BUILD_VERSION);
}
int32_t version2number(string version) {
    regex pattern("(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})");
    smatch result;
    if (!regex_match(version, result, pattern)) {
        _cout("cannot match version:" << version);
        return 0;
    }

    int32_t v = atoi(result[1].str().c_str()) * 1000 * 1000 +
                atoi(result[2].str().c_str()) * 1000 + atoi(result[3].str().c_str());
    return v;
}
