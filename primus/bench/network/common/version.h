#pragma once

#define PADO_MAJOR_VERSION 1 // 0~999
#define PADO_MINOR_VERSION 1 // 0~999
#define PADO_BUILD_VERSION 12 // 0~999

#include <inttypes.h>
#include <string>
int32_t pado_version_number();
std::string pado_version_string();

// return 0 if `version` not in the format: major{1,3}.minor{1,3}.build{1,3}
int32_t version2number(std::string version);

#ifndef VERSION_1_1_0
#define VERSION_1_1_0 1001000  // since 2023/08/16
#endif

#ifndef VERSION_1_1_1
#define VERSION_1_1_1 1001001
#endif
