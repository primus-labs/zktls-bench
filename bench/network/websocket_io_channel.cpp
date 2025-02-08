#include "websocket_io_channel.h"

std::map<uint16_t, int> WebSocketIO::listen_ports;
std::mutex WebSocketIO::listen_mutex;
map<string, IpPort> WebSocketIO::address_to_ip_port;
std::mutex WebSocketIO::address_mutex;

WebSocketIO::WebSocketIO(const char* address, int port) 
  : send_ctx(nullptr, FreeSendCtx), recv_ctx(nullptr, FreeRecvCtx)
     {
    if (port < 0 || port > 65535) {
        throw InvalidArgumentException("ws invalid port:" + to_string(port));
    }

    this->port = port;
    is_server = (address == nullptr);

    send_ctx.reset(NewSendCtx());
    recv_ctx.reset(NewRecvCtx());
    if (address == nullptr) {
#ifndef __EMSCRIPTEN__
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
                int _mysocket = socket(AF_INET, SOCK_STREAM, 0);
                if (_mysocket < 0) {
                    throw SocketError("ws socket error: " + get_errno_msg());
                }
                unique_ptr<TCPSocket> mysocket(new TCPSocket(_mysocket, true));
                int reuse = 1;
                setsockopt(mysocket->GetFd(), SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse,
                           sizeof(reuse));
                if (::bind(mysocket->GetFd(), (struct sockaddr*)&serv, sizeof(struct sockaddr)) <
                    0) {
                    throw SocketError("ws bind error: " + get_errno_msg());
                }
                if (listen(mysocket->GetFd(), 10) < 0) {
                    throw SocketError("ws listen error: " + get_errno_msg());
                }
                listen_socket = mysocket->GetFd();
                listen_ports[(uint16_t)port] = listen_socket;
                mysocket->SetCloseOnDelete(false);
            } else
                listen_socket = iter->second;
        }
        socklen_t socksize = sizeof(struct sockaddr_in);
        int _consocket = accept(listen_socket, (struct sockaddr*)&dest, &socksize);
        if (_consocket < 0) {
            throw SocketError("ws accept error: " + get_errno_msg());
        }
        consocket.reset(new TCPSocket(_consocket, true));
#endif
    } else {
#ifndef __EMSCRIPTEN__
        {
            std::unique_lock<std::mutex> lck(address_mutex);
            auto iter = address_to_ip_port.find(address);
            if (iter == address_to_ip_port.end()) {
                ip_port = parse_url(address);
                address_to_ip_port[address] = ip_port;
                _printf("ip:%s port:%d\n", ip_port.ip.c_str(), ip_port.port);
            }
            else {
                ip_port = iter->second;
            }
            protocol_type = ip_port.protocol_type;
        }
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = inet_addr(ip_port.ip.c_str());
        dest.sin_port = htons(ip_port.port);
        
        int _consocket = -1;
        while (1) {
            _consocket = socket(AF_INET, SOCK_STREAM, 0);

            if (connect(_consocket, (struct sockaddr*)&dest, sizeof(struct sockaddr)) ==
                0) {
                break;
            }

            close(_consocket);
            usleep(1000);
        }
        consocket.reset(new TCPSocket(_consocket, true));
#else
        channel_stream.reset(new EmscriptenStream(recv_ctx.get(), address, ConnectionType::CLIENT_TO_PADO,
                                                  emscripten_init_websocket_to_posix_socket_bridge2, GetRecvCtxForPado));
#endif
    }
}
void WebSocketIO::Init() {
    set_recv_timeout(120*1000);
    if (is_server) {
#ifndef __EMSCRIPTEN__
        channel_stream.reset(new SocketStream(consocket->GetFd(), ConnectionType::CLIENT_TO_PADO));
        ResponseWebSocketHandshake(channel_stream.get());
#endif
    }
    else {
#ifndef __EMSCRIPTEN__
        if (protocol_type == IpPort::WS) {
            channel_stream.reset(new SocketStream(consocket->GetFd(), ConnectionType::CLIENT_TO_PADO));
        }
        else if (protocol_type == IpPort::WSS) {
            channel_stream.reset(new SSLStream(consocket->GetFd(), ConnectionType::CLIENT_TO_PADO));
        }
        RequestWebSocketHandshake(channel_stream.get(), ip_port.host.c_str(), ip_port.uri.c_str());
        CheckWebSocketHandshake(channel_stream.get());
#else
        channel_stream->WaitReadyState();
#endif
    }
#ifndef __EMSCRIPTEN__
    set_nodelay();
#if RECORD_SEND_RECV
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_%d.log", is_server? "server": "client", consocket->GetFd());
    record_fp = fopen(filename, "w");
#endif
#if !USE_TCP_SOCKET_API
    if (protocol_type == IpPort::WS) {
        channel_stream.reset(new FileStream(consocket->GetFd(), ConnectionType::CLIENT_TO_PADO));
        consocket->SetCloseOnDelete(false);
    }
#endif
#endif
#if OUTPUT_PERF_DATA
    total_start = emp::clock_start();
    last_flush_time = emp::clock_start();
#endif
    start_async_flush();
    _printf("websocketio connected\n");
}

void WebSocketIO::print(const char* msg) {
#if OUTPUT_PERF_DATA
    double total_time = emp::time_from(total_start);
    double communication_cost = send_cost + recv_cost;
    double computation_cost = total_time - excluded_cost - communication_cost;
    if (msg) {
        _printf("%s\n", msg);
        if (strcmp(msg, "offline") == 0) {
            offline_send = send_bytes;
            offline_recv = recv_bytes;
            offline_rounds = (net_operation_change_count + 1) / 2;
            offline_communication_cost = communication_cost;
            offline_computation_cost = computation_cost;
        }
        else if (strcmp(msg, "online") == 0) {
            online_send = send_bytes;
            online_recv = recv_bytes;
            online_rounds = (net_operation_change_count + 1) / 2;
            online_communication_cost = communication_cost;
            online_computation_cost = computation_cost;
            _printf("online send bytes: %zu\n", online_send - offline_send);
            _printf("online recv bytes: %zu\n", online_recv - offline_recv);
            _printf("online rounds: %zu\n", online_rounds - offline_rounds);
            _printf("online communication cost: %.3fms\n", (online_communication_cost - offline_communication_cost) /1e3);
            _printf("online computation cost: %.3fms\n", (online_computation_cost - offline_computation_cost) / 1e3);
        }
        else if (strcmp(msg, "izk") == 0) {
            _printf("izk send bytes: %zu\n", send_bytes - online_send);
            _printf("izk recv bytes: %zu\n", recv_bytes - online_recv);
            _printf("izk rounds: %zu\n", (net_operation_change_count + 1) / 2 - online_rounds);
            _printf("izk communication cost: %.3fms\n", (communication_cost - online_communication_cost) / 1e3);
            _printf("izk computation cost: %.3fms\n", (computation_cost - online_computation_cost) / 1e3);
        }
    }
    _printf("websocket send=> %zu count %zu bytes cost %.3fms\n", send_cnt, send_bytes, send_cost/1e3);
    _printf("websocket recv=> %zu count %zu bytes cost %.3fms\n", recv_cnt, recv_bytes, recv_cost/1e3);
    _printf("websocket rounds=>%zu\n", (net_operation_change_count + 1) / 2);
    _printf("websocket time cost=> total time:%.3fms excluded cost:%.3fms communication cost:%.3fms computation_cost:%.3fms\n",
           total_time/1e3, excluded_cost/1e3, communication_cost/1e3, computation_cost/1e3);
    fflush(stdout);
    // send_cnt = 0;
    // recv_cnt = 0;
    // send_bytes = 0;
    // recv_bytes = 0;
    // net_operation_change_count = 0;
    // net_operation = NET_NONE;
#endif
}

void WebSocketIO::async_flush(WebSocketIO* io) {
    while (io->send_ctx->async || !EmptyFlushCtx(io->send_ctx->flush_ctx.get())) {
        AsyncFlushMessage(io->send_ctx.get(), io->channel_stream.get());
    }
}

void WebSocketIO::send_data_internal(const void* data, size_t len) {
#if OUTPUT_PERF_DATA
    auto start = emp::clock_start();
#endif
    if (len > 0) {
#if OUTPUT_PERF_DATA
        send_cnt++;
        send_bytes += (uint64_t)len;
        if (net_operation != NET_SEND) {
            net_operation_change_count++;
            net_operation = NET_SEND;
            // _printf("%d change to net send\n", consocket->GetFd());
        }
#endif
#if RECORD_SEND_RECV
        if (enable_record) {
            fprintf(record_fp, "SEND %d\n", (int)len);
        }
#endif
    }

    if (send_ctx->async) {
        AsyncSendMessage(send_ctx.get(), (char*)data, len, channel_stream.get());
    }
    else {
        SendMessage(send_ctx.get(), (char*)data, len, channel_stream.get());
    }
    if (len > 0) {
        has_sent = true;
#if OUTPUT_PERF_DATA
        double elapsed = emp::time_from(last_flush_time) / 1e3;
        if (elapsed > 10) {
            flush(true);
            has_sent = false;
            last_flush_time = emp::clock_start();
        }
        send_cost += emp::time_from(start);
#endif
    }
}

void WebSocketIO::recv_data_internal(void* data, size_t len) {
#if OUTPUT_PERF_DATA
    auto start = emp::clock_start();
    recv_cnt++;
    recv_bytes += (uint64_t)len;
    if (net_operation != NET_RECV) {
        net_operation_change_count++;
        net_operation = NET_RECV;
        // _printf("%d change to net recv\n", consocket->GetFd());
    }
#endif
    if (has_sent) {
        flush(true);
        has_sent = false;
#if OUTPUT_PERF_DATA
        last_flush_time = emp::clock_start();
#endif
    }
#if RECORD_SEND_RECV
    if (enable_record) {
        fprintf(record_fp, "RECV %d\n", (int)len);
    }
#endif

    RecvMessage(recv_ctx.get(), (char*)data, len, channel_stream.get());
#if OUTPUT_PERF_DATA
    recv_cost += emp::time_from(start);
#endif
}

