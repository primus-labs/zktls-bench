#ifndef EMP_WEBSOCKET_IO_CHANNEL
#define EMP_WEBSOCKET_IO_CHANNEL

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "pado_io_channel.h"
#include "common/smart_pointer.h"

#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
using namespace std;

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "ws.h"

#define USE_TCP_SOCKET_API 1 
#define RECORD_SEND_RECV 0
#define OUTPUT_PERF_DATA 0 

inline string get_errno_msg() {
    return " errno: " + to_string(errno) + ", errmsg:" + string(strerror(errno));
}

enum NET_OPERATION { NET_NONE, NET_SEND, NET_RECV };
class WebSocketIO : public PadoIOChannel<WebSocketIO> {
   public:
    bool is_server;
    static map<uint16_t, int> listen_ports;
    static std::mutex listen_mutex;
    static map<string, IpPort> address_to_ip_port;
    static std::mutex address_mutex;

    unique_ptr<TCPSocket> consocket = nullptr;
    bool has_sent = false;
    string addr;
    int port;
    StdUniquePtr<SendCtx>::Type send_ctx;
    StdUniquePtr<RecvCtx>::Type recv_ctx;
    unique_ptr<std::thread> flush_thread = nullptr;
    time_point<high_resolution_clock> last_flush_time;
#if RECORD_SEND_RECV
    FILE *record_fp = nullptr;
#endif
    bool enable_record = false;

#if OUTPUT_PERF_DATA
    size_t offline_send = 0;
    size_t offline_recv = 0;
    size_t offline_rounds = 0;
    size_t offline_communication_cost = 0;
    size_t offline_computation_cost = 0;

    size_t online_send = 0;
    size_t online_recv = 0;
    size_t online_rounds = 0;
    size_t online_communication_cost = 0;
    size_t online_computation_cost = 0;

    time_point<high_resolution_clock> excluded_start;
    time_point<high_resolution_clock> total_start;
    double send_cost = 0;
    double recv_cost = 0;
    double excluded_cost = 0;

    size_t send_cnt = 0;
    size_t send_bytes = 0;
    size_t recv_cnt = 0;
    size_t recv_bytes = 0;
    size_t net_operation_change_count = 0;
    NET_OPERATION net_operation = NET_NONE;
#endif
    IpPort ip_port;
    IpPort::ProtocolType protocol_type = IpPort::WS;
    unique_ptr<ChannelStream> channel_stream;
   private:
    WebSocketIO(const char* address, int port); 
   public:
    WebSocketIO(const char* address): WebSocketIO(address, 0) {}
    WebSocketIO(int port): WebSocketIO(nullptr, port) {}
    

    void Init();

    void set_recv_timeout(int timeout_msec) {
#ifndef __EMSCRIPTEN__
        int timeout_sec = timeout_msec/1000;
        int timeout_usec = (timeout_msec%1000)*1000;
        struct timeval timeout = {timeout_sec, timeout_usec};
        setsockopt(consocket->GetFd(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#endif
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

    void print(const char* msg = nullptr);

    void start_async_flush() {
        if (!send_ctx->async) {
            flush(true);
            send_ctx->async = true;
            flush_thread.reset(new std::thread(async_flush, this));
        }
    }

    void stop_async_flush() {
        if (!send_ctx->async) {
            return;
        }

        {
            unique_lock<std::mutex> lck(send_ctx->flush_ctx->mtx);
            send_ctx->async = false;
            send_ctx->flush_ctx->cond.notify_one();
        }
        flush_thread->join();
        flush_thread.reset(nullptr);
        flush(true);
    }

    ~WebSocketIO() {
        if (send_ctx->async) {
            stop_async_flush();
        }
        flush(true);
#ifndef __EMSCRIPTEN__
#if RECORD_SEND_RECV
        fclose(record_fp);
#endif
#else
        channel_stream->Close();
#endif
    }

    void set_nodelay() {
        const int one = 1;
        setsockopt(consocket->GetFd(), IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    }

    void set_delay() {
        const int zero = 0;
        setsockopt(consocket->GetFd(), IPPROTO_TCP, TCP_NODELAY, &zero, sizeof(zero));
    }

    static void async_flush(WebSocketIO* io);

    void flush(bool flag = true) {
        if (!flag)
            return;
#if RECORD_SEND_RECV
        if (enable_record) {
            fprintf(record_fp, "FLUSH 0\n");
        }
#endif
        send_data_internal(nullptr, 0);
        channel_stream->Flush();
    }

    void begin_exclude_cost() {
#if OUTPUT_PERF_DATA
        excluded_start = emp::clock_start();
#endif
    }

    void end_exclude_cost() {
#if OUTPUT_PERF_DATA
        excluded_cost += emp::time_from(excluded_start);
#endif
    }

    void send_data_internal(const void* data, size_t len);

    void recv_data_internal(void* data, size_t len);
};


#endif //WEBSOCKET_IO_CHANNEL
