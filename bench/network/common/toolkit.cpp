#include "toolkit.h"

#include <chrono>
#include <random>
#include <regex>
#include <string.h>
using namespace std;

bool is_little_endian() {
    union w {
        int a;
        char b;
    } c;
    c.a = 1;
    return (c.b == 1);
}

vector<string> split(const string& in, const string& delim) {
    regex re{delim};
    return vector<string>{sregex_token_iterator(in.begin(), in.end(), re, -1),
                          sregex_token_iterator()};
}

// return a hex-string with `len*2` characters
std::string generate_uid(int len) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string s;
    for (auto i = 0; i < len; i++) {
        const auto rc = dis(gen);
        char buf[8];
        snprintf(buf, 8, "%02x", rc);
        s += string(buf, 2);
    }
    return s;
}

std::string get_log_uuid(bool reset){
    const int LOG_UUID_LEN = 4;
    static string uid = generate_uid(LOG_UUID_LEN);
    if (reset){
        uid = generate_uid(LOG_UUID_LEN);
    }
    return uid;
}

std::string get_local_format_datetime(DTF dft) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const char* fmt_date = "%Y-%m-%d %H:%M:%S";
    const char* fmt_ms = "%s.%03d";
    if (dft == DTF::YYYYMMDDHHMMSSMS) {
        fmt_date = "%Y%m%d%H%M%S";
        fmt_ms = "%s%03d";
    }

    auto now = system_clock::now();
    auto timestamp = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;

    struct tm* info = localtime(&timestamp);
    char buf_datetime[24];
    memset(buf_datetime, 0, sizeof(buf_datetime));
    strftime(buf_datetime, 24, fmt_date, info);

    char buffer[32];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, 32, fmt_ms, buf_datetime, (int)ms);

    return std::string(buffer);
}
