#ifndef EMP_MY_IO_CHANNEL
#define EMP_MY_IO_CHANNEL

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <mutex>
#include <map>
#include "pado_io_channel.h"
using std::string;

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define RECORD_MSG_INFO 0

#include "common/runtime_error.h"

class EmpIO : public PadoIOChannel<EmpIO> {
   public:
    enum NET_OPERATION { NET_NONE, NET_SEND, NET_RECV };
    bool is_server;
    int mysocket;
    static map<uint16_t, int> listen_ports;
    static std::mutex listen_mutex;
    int consocket = -1;
    FILE* stream = nullptr;
    char* buffer = nullptr;
    bool has_sent = false;
    string addr;
    int port;
    uint64_t send_id = 0;
    uint64_t recv_id = 0;

    uint64_t send_cnt = 0;
    uint64_t send_bytes = 0;
    uint64_t recv_cnt = 0;
    uint64_t recv_bytes = 0;
    uint64_t net_operation_change_count = 0;
    NET_OPERATION net_operation = NET_NONE;

#if RECORD_MSG_INFO
    FILE* debug_file = nullptr;
#endif
    EmpIO(const char* address, int port, bool stop_listen = false, bool quiet = false) {
        if (port < 0 || port > 65535) {
            throw InvalidArgumentException("empio invalid port:" + to_string(port));
        }

        this->port = port;
        is_server = (address == nullptr);
        if (address == nullptr) {
            struct sockaddr_in dest;
            int listen_socket;
            {
                std::unique_lock<std::mutex> lck(listen_mutex);
                auto iter = listen_ports.find((uint16_t)port);
                if (iter == listen_ports.end()) {
                    struct sockaddr_in serv;
                    memset(&serv, 0, sizeof(serv));
                    serv.sin_family = AF_INET;
                    serv.sin_addr.s_addr =
                      htonl(INADDR_ANY);         /* set our address to any interface */
                    serv.sin_port = htons(port); /* set the server port number */
                    mysocket = socket(AF_INET, SOCK_STREAM, 0);
                    int reuse = 1;
                    setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse,
                               sizeof(reuse));
                    if (::bind(mysocket, (struct sockaddr*)&serv, sizeof(struct sockaddr)) <
                        0) {
                        throw SocketError("empio bind error");
                    }
                    if (listen(mysocket, 10) < 0) {
                        throw SocketError("empio listen error");
                    }
                    listen_socket = mysocket;
                    listen_ports[(uint16_t)port] = listen_socket;
                } else
                    listen_socket = iter->second;
            }
            socklen_t socksize = sizeof(struct sockaddr_in);
            consocket = accept(listen_socket, (struct sockaddr*)&dest, &socksize);
            if (consocket < 0) {
                throw SocketError("empio accept error");
            }
            if (stop_listen) {
                auto iter = listen_ports.find((uint16_t)port);
                if (iter != listen_ports.end()) {
                    listen_ports.erase(iter);
                }
                close(listen_socket);
            }
#if RECORD_MSG_INFO
            debug_file = fopen("myio_server.log", "w");
#endif
        } else {
            addr = string(address);

            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_addr.s_addr = inet_addr(address);
            dest.sin_port = htons(port);

            while (1) {
                consocket = socket(AF_INET, SOCK_STREAM, 0);

                if (connect(consocket, (struct sockaddr*)&dest, sizeof(struct sockaddr)) ==
                    0) {
                    break;
                }

                close(consocket);
                usleep(1000);
            }
#if RECORD_MSG_INFO
            debug_file = fopen("myio_client.log", "w");
#endif
        }
        set_nodelay();
        stream = fdopen(consocket, "wb+");
        buffer = new char[NETWORK_BUFFER_SIZE];
        memset(buffer, 0, NETWORK_BUFFER_SIZE);
        setvbuf(stream, buffer, _IOFBF, NETWORK_BUFFER_SIZE);
        if (!quiet)
            _printf("myio connected\n");
    }

    void sync() {
        int tmp = 0;
        if (is_server) {
            send_data_internal(&tmp, 1);
            recv_data_internal(&tmp, 1);
        } else {
            recv_data_internal(&tmp, 1);
            send_data_internal(&tmp, 1);
            flush(true);
        }
    }

    void print(const char* msg = nullptr) {
        if (msg) {
            _printf("%s\n", msg);
        }
        _printf("empio send=> %zu count %zu bytes\n", (size_t)send_cnt, (size_t)send_bytes);
        _printf("empio recv=> %zu count %zu bytes\n", (size_t)recv_cnt, (size_t)recv_bytes);
        _printf("empio rounds=>%zu\n", (size_t)(net_operation_change_count + 1) / 2);
        fflush(stdout);
        send_cnt = 0;
        recv_cnt = 0;
        send_bytes = 0;
        recv_bytes = 0;
        net_operation_change_count = 0;
        net_operation = NET_NONE;
    }

    ~EmpIO() {
        flush(true);
        fclose(stream);
        delete[] buffer;
    }

    void set_nodelay() {
        const int one = 1;
        setsockopt(consocket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    }

    void set_delay() {
        const int zero = 0;
        setsockopt(consocket, IPPROTO_TCP, TCP_NODELAY, &zero, sizeof(zero));
    }

    void flush(bool flag = true) {
        if (flag)
            fflush(stream);
    }

    void send_data_internal(const void* data, size_t len) {
        send_id++;
        send_cnt++;
        send_bytes += (uint64_t)len;
        if (net_operation != NET_SEND) {
            net_operation_change_count++;
            net_operation = NET_SEND;
        }
#if RECORD_MSG_INFO
        fprintf(debug_file, "send data id:%llu len:%llu\n", (uint64_t)send_id, (uint64_t)len);
        fflush(debug_file);
#endif
        size_t sent = 0;
        while (sent < len) {
            size_t res = fwrite(sent + (char*)data, 1, len - sent, stream);
            if (res > 0)
                sent += res;
            else
                error("net_send_data\n");
        }
        has_sent = true;
    }

    void recv_data_internal(void* data, size_t len) {
        recv_id++;
        recv_cnt++;
        recv_bytes += (uint64_t)len;
        if (net_operation != NET_RECV) {
            net_operation_change_count++;
            net_operation = NET_RECV;
        }
#if RECORD_MSG_INFO
        fprintf(debug_file, "recv data id: %llu len:%llu\n", (uint64_t)recv_id, (uint64_t)len);
        fflush(debug_file);
#endif
        if (has_sent)
            fflush(stream);
        has_sent = false;
        size_t sent = 0;
        while (sent < len) {
            size_t res = fread(sent + (char*)data, 1, len - sent, stream);
            if (res > 0)
                sent += res;
            else
                error("net_recv_data\n");
        }
    }
};


#endif //NETWORK_IO_CHANNEL
