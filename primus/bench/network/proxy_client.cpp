#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#if defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#endif

#include <memory>
#include <string.h>
#include <unistd.h>
#include "proxy_client.h"
#include "common/runtime_error.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#ifndef __EMSCRIPTEN__
typedef char EM_BOOL;
#define EM_TRUE 1
#endif

void init_openssl() {
    int ret = OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL);
    if (ret < 0) {
        throw RuntimeError("init ssl error");
    }
}
static void* memdup(const void* ptr, size_t sz) {
    if (!ptr)
        return 0;
    void* dup = malloc(sz);
    if (dup)
        memcpy(dup, ptr, sz);
    return dup;
}

// Each proxied socket call has at least the following data.
typedef struct SocketCallHeader {
    int callId;
    int function;
} SocketCallHeader;

// Each socket call returns at least the following data.
typedef struct SocketCallResultHeader {
    int callId;
    int ret;
    int errno_;
    // Buffer can contain more data here, conceptually:
    // uint8_t extraData[];
} SocketCallResultHeader;

typedef struct PosixSocketCallResult {
    struct PosixSocketCallResult* next;
    int callId;
    uint32_t operationCompleted;

    // Before the call has finished, this field represents the minimum expected
    // number of bytes that server will need to report back.  After the call has
    // finished, this field reports back the number of bytes pointed to by data,
    // >= the expected value.
    int bytes;

    // Result data:
    SocketCallResultHeader* data;
} PosixSocketCallResult;

// Shield multithreaded accesses to POSIX sockets functions in the program,
// namely the two variables 'bridgeSocket' and 'callResultHead' below.

// Socket handle for the connection from browser WebSocket to the sockets bridge
// proxy server.

// Stores a linked list of all currently pending sockets operations (ones that
// are waiting for a reply back from the sockets proxy server)

static PosixSocketCallResult* allocate_call_result(ProxyClientCtx* ctx, int expectedBytes) {
    unique_lock<std::mutex> lck(ctx->bridgeMtx);
    PosixSocketCallResult* b = (PosixSocketCallResult*)(malloc(sizeof(PosixSocketCallResult)));
    if (!b) {
        return 0;
    }
    b->callId = ctx->nextId++;
    b->bytes = expectedBytes;
    b->data = 0;
    b->operationCompleted = 0;
    b->next = 0;

    if (!ctx->callResultHead) {
        ctx->callResultHead = b;
    } else {
        PosixSocketCallResult* t = ctx->callResultHead;
        while (t->next)
            t = t->next;
        t->next = b;
    }
    return b;
}

static void free_call_result(PosixSocketCallResult* buffer) {
    if (buffer->data)
        free(buffer->data);
    free(buffer);
}

static PosixSocketCallResult* pop_call_result(ProxyClientCtx* ctx, int callId) {
    unique_lock<std::mutex> lck(ctx->bridgeMtx);
    PosixSocketCallResult* prev = 0;
    PosixSocketCallResult* b = ctx->callResultHead;
    while (b) {
        if (b->callId == callId) {
            if (prev)
                prev->next = b->next;
            else
                ctx->callResultHead = b->next;
            b->next = 0;
            return b;
        }
        prev = b;
        b = b->next;
    }
    return 0;
}

static void pop_all_call_results(ProxyClientCtx* ctx) {
    unique_lock<std::mutex> lck(ctx->bridgeMtx);
    PosixSocketCallResult* prev = nullptr;
    PosixSocketCallResult* h = ctx->callResultHead;

    while (h) {
        prev = h;
        h = h->next;

        free_call_result(prev);
    }
    ctx->callResultHead = nullptr;
}

static EM_BOOL BridgeSocketOnMessage(ProxyClientCtx* ctx, const char* data, size_t numBytes, uint64_t callId) {
    SocketCallResultHeader error_data;
    if (numBytes < sizeof(SocketCallResultHeader)) {
        error_data.callId = callId;
        error_data.ret = -1;
        data = (const char*)&error_data;
        numBytes = sizeof(error_data);
    }

    SocketCallResultHeader* header = (SocketCallResultHeader*)data;

    PosixSocketCallResult* b = pop_call_result(ctx, header->callId);
    if (!b) {
        // TODO: Craft a socket result that signifies a failure, and wake the listening thread
        assert(false);
        return EM_TRUE;
    }

    if ((int)numBytes < b->bytes) {
        // TODO: Craft a socket result that signifies a failure, and wake the listening thread
        assert(false);
        return EM_TRUE;
    }

    b->bytes = numBytes;
    b->data = (SocketCallResultHeader*)memdup(data, numBytes);

    if (!b->data) {
        assert(false);
        return EM_TRUE;
    }

    if (b->operationCompleted != 0 && b->operationCompleted != 2) {
        assert(false);
    }

    std::unique_lock<std::mutex> lck(ctx->operationCompletedMtx);
    if (b->operationCompleted == 0) {
        b->operationCompleted = 1;
        ctx->operationCompletedCond.notify_all();
    }
    else {
        free_call_result(b);
    }

    return EM_TRUE;
}
ProxyClientCtx::ProxyClientCtx(ClientType type)
    : 
#ifndef __EMSCRIPTEN__
      network_recv_ctx(NewRecvCtx(), FreeRecvCtx),
#endif
      send_ctx(NewSendProxyCtx(), FreeSendProxyCtx),
      recv_ctx(NewRecvProxyCtx(), FreeRecvProxyCtx) {
#ifndef __EMSCRIPTEN__
          network_recv_ctx->is_chunk = true;
#endif
          client_type = type;
      }

#ifndef __EMSCRIPTEN__
void PutCallId(ProxyClientCtx* ctx, uint64_t id);
#endif

static bool wait_for_call_result(ProxyClientCtx* ctx, PosixSocketCallResult* b) {
#ifndef __EMSCRIPTEN__
    PutCallId(ctx, (uint64_t)b->callId);
#endif
    std::unique_lock<std::mutex> lck(ctx->operationCompletedMtx);
    while (ctx->waitOperationCompleted && !b->operationCompleted) {
        ctx->operationCompletedCond.wait(lck);
    }
    if (b->operationCompleted != 1) {
        b->operationCompleted = 2;
        return false;
    }
    return true;
}

void cancel_wait_for_call_result(ProxyClientCtx* ctx) {
    std::unique_lock<std::mutex> lck(ctx->operationCompletedMtx);
    ctx->waitOperationCompleted = false;
    ctx->operationCompletedCond.notify_all();
}

#ifndef __EMSCRIPTEN__
void process_network(ProxyClientCtx* ctx) {
    ctx->waitNextCallId = true;
    ctx->channel_stream->SetRecvTimeout(1 * 1000); // setted for cancel when abnormal
    while (1) {
        unique_ptr<CallId> current;
        {
            std::unique_lock<std::mutex> lck(ctx->callIdMtx);
            while (ctx->waitNextCallId && ctx->callIdHeader == nullptr) {
                ctx->callIdCond.wait(lck);
            }
            if (ctx->callIdHeader == nullptr)
                break;
            CallId* nextCallId = ctx->callIdHeader->next;
            current.reset(ctx->callIdHeader);
            ctx->callIdHeader = nextCallId;
        }

        RecvList* recv_list = nullptr;
        try {
            recv_list = RecvMessage(ctx->network_recv_ctx.get(), (uint64_t)current->callId, ctx->channel_stream.get());
        }
        catch (...) {
        }
        if (!recv_list)
            break;
        RecvBuffer* recv_buffer = (RecvBuffer*)recv_list->data;

        BridgeSocketOnMessage(ctx, recv_buffer->payload, recv_buffer->length, (uint64_t)current->callId);
        free(recv_list);
    }
}

void PutCallId(ProxyClientCtx* ctx, uint64_t id) {
    CallId* callId = new CallId;
    callId->next = nullptr;
    callId->callId = id;

    std::unique_lock<std::mutex> lck(ctx->callIdMtx);
    if (ctx->callIdHeader == nullptr)
        ctx->callIdHeader = callId;
    else {
        CallId* current = ctx->callIdHeader;
        while (current->next)
            current = current->next;
        current->next = callId;
    }
    ctx->callIdCond.notify_one();
}
#endif

void set_shutdown_flag(ProxyClientCtx* ctx) {
    ctx->is_shutdowned = true;
}

static void OnRecvBytes(ProxyClientCtx* ctx, size_t num_bytes) {
    std::unique_lock<std::mutex> lck(ctx->bytes_mtx);
    ctx->recv_bytes += num_bytes;  // remove header(id, length)
    if (ctx->recv_bytes >= ctx->minimal_bytes) {
        ctx->bytes_cond.notify_one();
    }
}

void WaitForBytes(ProxyClientCtx* ctx) {
    {
        std::unique_lock<std::mutex> lck(ctx->bytes_mtx);
        while (!ctx->recv_zero && ctx->recv_bytes < ctx->minimal_bytes) {
            ctx->bytes_cond.wait(lck);
        }
    }
    if (ctx->recv_bytes < ctx->minimal_bytes) {
        throw NetworkError("peer close while wait for bytes");
    }
}

void process_send(ProxyClientCtx* ctx) {
    while (1) {
        PosixSocketCallResult* b = (PosixSocketCallResult*)GetFromSendProxyCtx(ctx->send_ctx.get());
        if (b == nullptr)
            break;
        bool normal = wait_for_call_result(ctx, b);
        if (!normal)
            break;
        int ret = b->data->ret;
        if (ret != 0)
            errno = b->data->errno_;
        free_call_result(b);
        ctx->last_send_time = emp::clock_start();
    }
}

ssize_t do_recv(ProxyClientCtx* ctx, int socket, void* buffer, size_t length, int flags);
void process_recv(ProxyClientCtx* ctx) {
    unique_ptr<char[]> buf(new char[SEND_BUFFER_SIZE]);
    while (1) {
        ssize_t ret = do_recv(ctx, ctx->recv_ctx->sock, buf.get(), SEND_BUFFER_SIZE, 0);
        if (ret <= 0) {
            if (ret == -1 && (errno == EAGAIN || errno == EINTR)) {
                continue;
            }
            SetRecvProxyCtx(ctx->recv_ctx.get(), false);
            if (ctx->is_shutdowned) {
                _printf("[%s] info recv 0, peer close, timeout:%.3f\n", to_string(ctx->client_type).c_str(), emp::time_from(ctx->last_send_time) / 1e3);
            }
            else {
                _printf("[%s] error recv 0, peer close, timeout:%.3f\n", to_string(ctx->client_type).c_str(), emp::time_from(ctx->last_send_time) / 1e3);
            }
            std::unique_lock<std::mutex> lck(ctx->bytes_mtx);
            ctx->recv_zero = true;
            ctx->bytes_cond.notify_one();
            break;
        }
        PutToRecvProxyCtx(ctx->recv_ctx.get(), buf.get(), ret);
        OnRecvBytes(ctx, ret);
    }
}

#ifdef __EMSCRIPTEN__
static EM_BOOL bridge_socket_on_message3(int eventType,
                                         const EmscriptenWebSocketMessageEvent* websocketEvent,
                                         void* userData) {
    ProxyClientCtx* ctx = (ProxyClientCtx*)userData;
    if (websocketEvent->numBytes < sizeof(SocketCallResultHeader)) {
        assert(false);
        return EM_TRUE;
    }

    SocketCallResultHeader* header = (SocketCallResultHeader*)websocketEvent->data;
    return BridgeSocketOnMessage(ctx, (const char*)websocketEvent->data, websocketEvent->numBytes, header->callId);
}
static EM_BOOL bridge_socket_on_error3(int eventType,
                                       const EmscriptenWebSocketErrorEvent* websocketEvent,
                                       void* userData) {
    ProxyClientCtx* ctx = (ProxyClientCtx*)userData;
    ctx->proxy_close = true;
    {
        std::unique_lock<std::mutex> lck(ctx->bytes_mtx);
        ctx->recv_zero = true;
        ctx->bytes_cond.notify_one();
    }

    RecvCtx* recv_ctx = ctx->recv_ctx->inner_ctx.get();
    unique_lock<std::mutex> lck(recv_ctx->mtx);
    recv_ctx->recv_zero = true;
    recv_ctx->cond.notify_one();
    _printf("connect proxy error\n");
    fflush(stdout);
    return EM_TRUE;
}
static EM_BOOL bridge_socket_on_open3(int eventType,
                                       const EmscriptenWebSocketOpenEvent* websocketEvent,
                                       void* userData) {
    _printf("connect proxy successfully\n");
    fflush(stdout);
    return EM_TRUE;
}
static EM_BOOL bridge_socket_on_close3(int eventType,
                                       const EmscriptenWebSocketCloseEvent* websocketEvent,
                                       void* userData) {
    ProxyClientCtx* ctx = (ProxyClientCtx*)userData;
    ctx->proxy_close = true;
    {
        std::unique_lock<std::mutex> lck(ctx->bytes_mtx);
        ctx->recv_zero = true;
        ctx->bytes_cond.notify_one();
    }

    RecvCtx* recv_ctx = ctx->recv_ctx->inner_ctx.get();
    unique_lock<std::mutex> lck(recv_ctx->mtx);
    recv_ctx->recv_zero = true;
    recv_ctx->cond.notify_one();
    _printf("proxy close connectioni, code:%d, reason:%s\n", websocketEvent->code, websocketEvent->reason);
    fflush(stdout);
    return EM_TRUE;
}

EMSCRIPTEN_WEBSOCKET_T emscripten_init_websocket_to_posix_socket_bridge3(void* user_ctx, const char* bridgeUrl) {
    ProxyClientCtx* ctx = (ProxyClientCtx*)user_ctx;
    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = bridgeUrl;
    int bridgeSocket = emscripten_websocket_new(&attr);
    emscripten_websocket_set_onmessage_callback_on_thread(
      bridgeSocket, ctx, bridge_socket_on_message3,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
    emscripten_websocket_set_onerror_callback_on_thread(
      bridgeSocket, ctx, bridge_socket_on_error3,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
    emscripten_websocket_set_onopen_callback_on_thread(
      bridgeSocket, ctx, bridge_socket_on_open3,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
    emscripten_websocket_set_onclose_callback_on_thread(
      bridgeSocket, ctx, bridge_socket_on_close3,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);

    return bridgeSocket;
}
RecvCtx* GetRecvCtxForServer(void* user_ctx) {
    ProxyClientCtx *ctx = (ProxyClientCtx*)user_ctx;
    return ctx->recv_ctx->inner_ctx.get();
}
#endif

void PlainWebSocketProxy::Init(const char* bridgeUrl) {
#ifndef __EMSCRIPTEN__
    IpPort ip_port = parse_url(bridgeUrl);
    const char* address = ip_port.ip.c_str();
    int port = ip_port.port;
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(address);
    dest.sin_port = htons(port);
    
    int bridgeSocket = -1;
    while (1) {
        bridgeSocket = socket(AF_INET, SOCK_STREAM, 0);

        if (connect(bridgeSocket, (struct sockaddr*)&dest, sizeof(struct sockaddr)) == 0) {
            break;
        }

        close(bridgeSocket);
        usleep(1000);
    }
    if (ip_port.protocol_type == IpPort::WS)
        ctx->channel_stream.reset(new WebSocketStream(bridgeSocket, ConnectionType::CLIENT_TO_TLS_SERVER));
    else if (ip_port.protocol_type == IpPort::WSS) {
        ctx->channel_stream.reset(new SSLStream(bridgeSocket, ConnectionType::CLIENT_TO_TLS_SERVER));
    }
    RequestWebSocketHandshake(ctx->channel_stream.get(), ip_port.host.c_str(), ip_port.uri.c_str());
    CheckWebSocketHandshake(ctx->channel_stream.get());
    ctx->network_thread.reset(new std::thread(process_network, ctx.get()));
#else
    ctx->channel_stream.reset(new EmscriptenStream(ctx.get(), bridgeUrl, ConnectionType::CLIENT_TO_TLS_SERVER,
                                                   emscripten_init_websocket_to_posix_socket_bridge3, GetRecvCtxForServer));
    ctx->channel_stream->WaitReadyState();
#endif                       
}

WebSocketProxy* NewPlainWebSocketProxy(ClientType clientType) {
    return new PlainWebSocketProxy(clientType);
}

WebSocketProxy* NewSecureWebSocketProxy(ClientType clientType) {
    return new SecureWebSocketProxy(clientType);
}

#define POSIX_SOCKET_MSG_SOCKET 1
#define POSIX_SOCKET_MSG_SOCKETPAIR 2
#define POSIX_SOCKET_MSG_SHUTDOWN 3
#define POSIX_SOCKET_MSG_BIND 4
#define POSIX_SOCKET_MSG_CONNECT 5
#define POSIX_SOCKET_MSG_LISTEN 6
#define POSIX_SOCKET_MSG_ACCEPT 7
#define POSIX_SOCKET_MSG_GETSOCKNAME 8
#define POSIX_SOCKET_MSG_GETPEERNAME 9
#define POSIX_SOCKET_MSG_SEND 10
#define POSIX_SOCKET_MSG_RECV 11
#define POSIX_SOCKET_MSG_SENDTO 12
#define POSIX_SOCKET_MSG_RECVFROM 13
#define POSIX_SOCKET_MSG_SENDMSG 14
#define POSIX_SOCKET_MSG_RECVMSG 15
#define POSIX_SOCKET_MSG_GETSOCKOPT 16
#define POSIX_SOCKET_MSG_SETSOCKOPT 17
#define POSIX_SOCKET_MSG_GETADDRINFO 18
#define POSIX_SOCKET_MSG_GETNAMEINFO 19

#define MAX_SOCKADDR_SIZE 256
#define MAX_OPTIONVALUE_SIZE 16

static void check_network(ProxyClientCtx* ctx) {
    if (ctx->proxy_close) {
        throw NetworkError("connection to proxy broken");
    }
    if (ctx->recv_zero || ctx->recv_ctx->inner_ctx->recv_zero) {
        throw NetworkError("connection to tls server broken");
    }

}

int getaddrinfo3(ProxyClientCtx* ctx,
                const char* node,
                const char* service,
                const struct addrinfo* hints,
                struct addrinfo** res) {
#define MAX_NODE_LEN 2048
#define MAX_SERVICE_LEN 128

  struct {
    SocketCallHeader header;
    char node[MAX_NODE_LEN]; // Arbitrary max length limit
    char service[MAX_SERVICE_LEN]; // Arbitrary max length limit
    int hasHints;
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
  } d;

  typedef struct ResAddrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    int/*socklen_t*/ ai_addrlen;
    uint8_t /*sockaddr **/ ai_addr[];
  } ResAddrinfo;

  typedef struct Result {
    SocketCallResultHeader header;
    char ai_canonname[MAX_NODE_LEN];
    int addrCount;
    uint8_t /*ResAddrinfo[]*/ addr[];
  } Result;

  memset(&d, 0, sizeof(d));
  PosixSocketCallResult *b = allocate_call_result(ctx, sizeof(Result));
  d.header.callId = b->callId;
  d.header.function = POSIX_SOCKET_MSG_GETADDRINFO;
  if (node) {
    assert(strlen(node) <= MAX_NODE_LEN-1);
    strncpy(d.node, node, MAX_NODE_LEN-1);
  }
  if (service) {
    assert(strlen(service) <= MAX_SERVICE_LEN-1);
    strncpy(d.service, service, MAX_SERVICE_LEN-1);
  }
  d.hasHints = !!hints;
  if (hints) {
    d.ai_flags = hints->ai_flags;
    d.ai_family = hints->ai_family;
    d.ai_socktype = hints->ai_socktype;
    d.ai_protocol = hints->ai_protocol;
  }

  check_network(ctx);
  ctx->channel_stream->SendEx(&d, sizeof(d));

  bool normal = wait_for_call_result(ctx, b);
  if (!normal) {
      return -1;
  }
  int ret = b->data->ret;
  if (ret == 0) {
    if (res) {
      Result *r = (Result*)b->data;
      uint8_t *raiAddr = (uint8_t*)&r->addr[0];
      struct addrinfo *results = (struct addrinfo*)malloc(sizeof(struct addrinfo)*r->addrCount);
      for (size_t i = 0; i < (size_t)r->addrCount; ++i) {
        ResAddrinfo *rai = (ResAddrinfo*)raiAddr;
        results[i].ai_flags = rai->ai_flags;
        results[i].ai_family = rai->ai_family;
        results[i].ai_socktype = rai->ai_socktype;
        results[i].ai_protocol = rai->ai_protocol;
        results[i].ai_addrlen = rai->ai_addrlen;
        results[i].ai_addr = (struct sockaddr *)malloc(results[i].ai_addrlen);
        memcpy(results[i].ai_addr, rai->ai_addr, results[i].ai_addrlen);
        results[i].ai_canonname = (i == 0) ? strdup(r->ai_canonname) : 0;
        results[i].ai_next = i+1 < (size_t)r->addrCount ? &results[i+1] : 0;
#if 0
        _printf("%d: ai_flags=%d, ai_family=%d, ai_socktype=%d, ai_protocol=%d, ai_addrlen=%d, ai_addr=", (int)i, results[i].ai_flags, results[i].ai_family, results[i].ai_socktype, results[i].ai_protocol, results[i].ai_addrlen);
        for (size_t j = 0; j < results[i].ai_addrlen; ++j)
          _printf(" %02X", ((uint8_t*)results[i].ai_addr)[j]);
        _printf(",ai_canonname=%s, ai_next=%p\n", results[i].ai_canonname, results[i].ai_next);
#endif
        raiAddr += sizeof(ResAddrinfo) + rai->ai_addrlen;
      }
      *res = results;
    }
  } else {
    errno = b->data->errno_;
    if (res) *res = 0;
  }
  free_call_result(b);

  return ret;
}

void freeaddrinfo3(struct addrinfo *res) {
  for (struct addrinfo *r = res; r; r = r->ai_next) {
    free(r->ai_canonname);
    free(r->ai_addr);
  }
  free(res);
}

int socket3(ProxyClientCtx* ctx, int domain, int type, int protocol) {
    struct {
        SocketCallHeader header;
        int domain;
        int type;
        int protocol;
    } d;

    PosixSocketCallResult* b = allocate_call_result(ctx, sizeof(SocketCallResultHeader));
    d.header.callId = b->callId;
    d.header.function = POSIX_SOCKET_MSG_SOCKET;
    d.domain = domain;
    d.type = type;
    d.protocol = protocol;
    check_network(ctx);
    ctx->channel_stream->SendEx(&d, sizeof(d));

    bool normal = wait_for_call_result(ctx, b);
    if (!normal) return -1;
    int ret = b->data->ret;
    if (ret < 0)
        errno = b->data->errno_;
    free_call_result(b);
    return ret;
}

int connect3(ProxyClientCtx* ctx, int socket, const struct sockaddr* address, socklen_t address_len) {
    typedef struct Data {
        SocketCallHeader header;
        int socket;
        uint32_t /*socklen_t*/ address_len;
        uint8_t address[];
    } Data;
    int numBytes = sizeof(Data) + address_len;
    Data* d = (Data*)malloc(numBytes);

    PosixSocketCallResult* b = allocate_call_result(ctx, sizeof(SocketCallResultHeader));
    d->header.callId = b->callId;
    d->header.function = POSIX_SOCKET_MSG_CONNECT;
    d->socket = socket;
    d->address_len = address_len;
    if (address)
        memcpy(d->address, address, address_len);
    else
        memset(d->address, 0, address_len);
    check_network(ctx);
    ctx->channel_stream->SendEx(d, numBytes);

    bool normal = wait_for_call_result(ctx, b);
    if (!normal) return -1;
    int ret = b->data->ret;
    if (ret != 0)
        errno = b->data->errno_;
    free_call_result(b);
    if (ret == 0) {
        ctx->send_thread.reset(new std::thread(process_send, ctx));

        ctx->recv_ctx->sock = socket;
        ctx->recv_thread.reset(new std::thread(process_recv, ctx));
    }

    free(d);
    return ret;
}

ssize_t send3(ProxyClientCtx* ctx, int socket, const void* message, size_t length, int flags) {
    typedef struct MSG {
        SocketCallHeader header;
        int socket;
        uint32_t /*size_t*/ length;
        int flags;
        uint8_t message[];
    } MSG;
    size_t sz = sizeof(MSG) + length;
    MSG* d = (MSG*)malloc(sz);

    PosixSocketCallResult* b = allocate_call_result(ctx, sizeof(SocketCallResultHeader));
    d->header.callId = b->callId;
    d->header.function = POSIX_SOCKET_MSG_SEND;
    d->socket = socket;
    d->length = length;
    d->flags = flags;
    if (message)
        memcpy(d->message, message, length);
    else
        memset(d->message, 0, length);
    if (ctx->need_wait) {
        unique_lock<std::mutex> lck(ctx->bytes_mtx);
        ctx->recv_bytes = 0;
        ctx->minimal_bytes = ctx->wait_bytes;
        ctx->need_wait = false;
    }
    check_network(ctx);
    ctx->channel_stream->SendEx(d, sz);

    PutToSendProxyCtx(ctx->send_ctx.get(), b);

    free(d);
    return length;
}

ssize_t do_recv(ProxyClientCtx* ctx, int socket, void* buffer, size_t length, int flags) {
    if (ctx->recv_zero || ctx->recv_ctx->inner_ctx->recv_zero) {
        return -1;
    }

    struct {
        SocketCallHeader header;
        int socket;
        uint32_t /*size_t*/ length;
        int flags;
    } d;

    PosixSocketCallResult* b = allocate_call_result(ctx, sizeof(SocketCallResultHeader));
    d.header.callId = b->callId;
    d.header.function = POSIX_SOCKET_MSG_RECV;
    d.socket = socket;
    d.length = length;
    d.flags = flags;
    ctx->channel_stream->SendEx(&d, sizeof(d));

    bool normal = wait_for_call_result(ctx, b);
    if (!normal) {
        return -1;
    }
    int ret = b->data->ret;
    if (ret >= 0) {
        typedef struct Result {
            SocketCallResultHeader header;
            uint8_t data[];
        } Result;
        Result* r = (Result*)b->data;
        if (buffer)
            memcpy(buffer, r->data, MIN(ret, (int)length));
    } else {
        errno = b->data->errno_;
    }
    free_call_result(b);

    return ret;
}

ssize_t recv3(ProxyClientCtx* ctx, int socket, void* buffer, size_t length, int flags) {
    ssize_t ret = RecvFromRecvProxyCtx(ctx->recv_ctx.get(), (char*)buffer, length);
    return ret;
}
int shutdown3(ProxyClientCtx* ctx, int socket, int how) {
    struct {
        SocketCallHeader header;
        int socket;
        int how;
    } d;

    PosixSocketCallResult* b = allocate_call_result(ctx, sizeof(SocketCallResultHeader));
    d.header.callId = b->callId;
    d.header.function = POSIX_SOCKET_MSG_SHUTDOWN;
    d.socket = socket;
    d.how = how;
    check_network(ctx);
    ctx->channel_stream->SendEx(&d, sizeof(d));

    bool normal = wait_for_call_result(ctx, b);
    if (!normal) return -1;
    int ret = b->data->ret;
    if (ret != 0)
        errno = b->data->errno_;
    free_call_result(b);

    return ret;
}

ProxyClientCtx::~ProxyClientCtx() {
    _printf("begin finalize proxy\n");
    if (this->send_thread != nullptr) {
        SetSendProxyCtx(this->send_ctx.get(), false);
        this->send_thread->join();
    }
    _printf("finalize send thread\n");

    if (this->recv_thread != nullptr) {
        cancel_wait_for_call_result(this);
        this->recv_thread->join();
    }
    _printf("finalize recv thread\n");
#ifndef __EMSCRIPTEN__
    if (this->network_thread != nullptr) {
        this->channel_stream->StopRecv();
        {
            std::unique_lock<std::mutex> lck(this->callIdMtx);
            this->waitNextCallId = false;
            this->callIdCond.notify_one();
        }
        this->network_thread->join();
    }
#endif
    this->channel_stream->Close();
    _printf("finalize network thread\n");
    pop_all_call_results(this);
}
void PutToSendProxyCtx(SendProxyCtx* ctx, void* p) {
    std::unique_lock<std::mutex> lck(ctx->mtx);
    SendResult* s = new SendResult;
    s->next = nullptr;
    s->result = p;
    if (ctx->list == nullptr)
        ctx->list = s;
    else {
        SendResult* c = ctx->list;
        while (c->next)
            c = c->next;
        c->next = s;
    }

    ctx->cond.notify_one();
}

void* GetFromSendProxyCtx(SendProxyCtx* ctx) {
    std::unique_lock<std::mutex> lck(ctx->mtx);
    while (ctx->valid && ctx->list == nullptr)
        ctx->cond.wait(lck);
    if (ctx->list == nullptr)
        return nullptr;
    SendResult* next = ctx->list->next;
    SendResult* current = ctx->list;
    ctx->list = next;

    void* result = current->result;
    delete current;
    return result;
}

SendProxyCtx* NewSendProxyCtx() {
    SendProxyCtx* p = new SendProxyCtx;
    return p;
}

void FreeSendProxyCtx(SendProxyCtx* p) { delete p; }

void SetSendProxyCtx(SendProxyCtx* ctx, bool valid) {
    std::unique_lock<std::mutex> lck(ctx->mtx);
    ctx->valid = valid;
    ctx->cond.notify_one();
}
RecvProxyCtx* NewRecvProxyCtx() {
    RecvProxyCtx* ctx = new RecvProxyCtx;
    ctx->inner_ctx.reset(NewRecvCtx());
    return ctx;
}

void FreeRecvProxyCtx(RecvProxyCtx* ctx) {
    delete ctx;
}

void SetRecvProxyCtx(RecvProxyCtx* ctx, bool valid) {
    std::unique_lock<std::mutex> lck(ctx->inner_ctx->mtx);
    ctx->inner_ctx->recv_zero = !valid;
    ctx->inner_ctx->cond.notify_one();
}
void PutToRecvProxyCtx(RecvProxyCtx* ctx, const char* buf, size_t len) {
    std::unique_lock<std::mutex> lck(ctx->inner_ctx->mtx);
    PutToRecvCtx(ctx->inner_ctx.get(), ++ctx->id, buf, len);

    ctx->inner_ctx->cond.notify_one();
}

ssize_t RecvFromRecvProxyCtx(RecvProxyCtx* ctx, char* buf, size_t len) {
    RecvCtx* c = ctx->inner_ctx.get();
    size_t recv_bytes = 0;
    while (recv_bytes < len) {
        {
            std::unique_lock<std::mutex> lck(c->mtx);
            while ((c->info == nullptr || !c->info->valid) && !c->recv_zero)
                c->cond.wait(lck);
    
            if (c->recv_zero && EmptyRecvCtx(c)) {
                break;
            }
            size_t ret = RecvFromRecvCtx(c, buf + recv_bytes, len - recv_bytes);
            recv_bytes += ret;
        }

    }
    return recv_bytes;
}

