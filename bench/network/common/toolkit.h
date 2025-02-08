#pragma once

#include <string>
#include <vector>
#include "macro.h"

bool is_little_endian();
std::vector<std::string> split(const std::string& in, const std::string& delim);
std::string generate_uid(int len);
std::string get_log_uuid(bool reset = false);

enum class DTF {             // DateTimeFormat
    YYYY_MM_DD_HH_MM_SS_MIS, // "2023-12-19 01:23:45.678"
    YYYYMMDDHHMMSSMS,        // "20231219012345678"
};
std::string get_local_format_datetime(DTF dft = DTF::YYYY_MM_DD_HH_MM_SS_MIS);
