#include "emp_io_channel.h"

std::map<uint16_t, int> EmpIO::listen_ports;
std::mutex EmpIO::listen_mutex;
