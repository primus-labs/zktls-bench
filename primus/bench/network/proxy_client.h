#include <mutex>
#include <condition_variable>
#include <thread>
#include "ws.h"
#include <emp-tool/emp-tool.h>
#include "common/smart_pointer.h"
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netdb.h>
#include "network.h"

static inline string to_string(ClientType client_type) {
    switch (client_type) {
        case ClientType::MPC:
            return "mpc client";
        case ClientType::PLAIN:
            return "plain client";
    }
    return "";
}

#ifdef __cplusplus
extern "C" {
#endif
struct PosixSocketCallResult;
struct SendProxyCtx;
struct RecvProxyCtx;
#ifndef __EMSCRIPTEN__
struct CallId {
    struct CallId* next;
    uint64_t callId;
};
#endif

struct ProxyClientCtx {
#ifndef __EMSCRIPTEN__
    CallId* callIdHeader = nullptr;
    std::mutex callIdMtx;
    std::condition_variable callIdCond;
    bool waitNextCallId = true;
    unique_ptr<std::thread> network_thread;
    StdUniquePtr<RecvCtx>::Type network_recv_ctx;
#endif
    unique_ptr<ChannelStream> channel_stream;
    std::mutex bridgeMtx;
    int proxySocket = -1;
    PosixSocketCallResult* callResultHead = 0;
    int nextId = 1;
    bool waitOperationCompleted = true;
    std::mutex operationCompletedMtx;
    std::condition_variable operationCompletedCond;
    StdUniquePtr<SendProxyCtx>::Type send_ctx;
    StdUniquePtr<RecvProxyCtx>::Type recv_ctx;
    unique_ptr<std::thread> send_thread;
    unique_ptr<std::thread> recv_thread;
    time_point<high_resolution_clock> last_send_time;

    std::mutex bytes_mtx;
    std::condition_variable bytes_cond;
    uint64_t recv_bytes = 0;
    uint64_t minimal_bytes = 0;
    
    bool need_wait = false;
    uint64_t wait_bytes = 0;

    bool is_shutdowned = false;
    bool recv_zero = false;
    bool proxy_close = false;

    ClientType client_type;
   
    ProxyClientCtx(ClientType client_type);
    ~ProxyClientCtx();

};
int emscripten_init_websocket_to_posix_socket_bridge3(ProxyClientCtx* ctx, const char* bridgeUrl);
int getaddrinfo3(ProxyClientCtx* ctx,
                const char* node,
                const char* service,
                const struct addrinfo* hints,
                struct addrinfo** res); 
void freeaddrinfo3(struct addrinfo *res);
int socket3(ProxyClientCtx* ctx, int domain, int type, int protocol);
int connect3(ProxyClientCtx* ctx, int socket, const struct sockaddr* address, socklen_t address_len);
ssize_t send3(ProxyClientCtx* ctx, int socket, const void* message, size_t length, int flags);
ssize_t recv3(ProxyClientCtx* ctx, int socket, void* buffer, size_t length, int flags);
int shutdown3(ProxyClientCtx* ctx, int socket, int how);
void WaitForBytes(ProxyClientCtx* ctx);
void set_shutdown_flag(ProxyClientCtx* ctx);

#ifdef __cplusplus
}
#endif



#define PROXY_SOCKET_OFFSET 1024
class PlainWebSocketProxy : public WebSocketProxy{
public:
    PlainWebSocketProxy(ClientType client_type): ctx(new ProxyClientCtx(client_type)) {}
    virtual ~PlainWebSocketProxy() {}
    ClientType GetClientType() override {
        return ctx->client_type;
    }

    void Init(const char* bridgeUrl) override;

    int Socket() override {
        fd = socket3(ctx.get(), AF_INET, SOCK_STREAM, 0);
        return fd;
    }

    int Connect(const char* ip, int port) override {
        struct sockaddr_in server;
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &server.sin_addr) < 0) {
            _printf("pton error\n");
            throw SocketError("ws inet_pton error");
        }
    
        int ret = connect3(ctx.get(), fd, (struct sockaddr*)&server, sizeof(server));
        if (ret < 0) {
            _printf("connect error %s\n", strerror(errno));
            throw SocketError("ws connect error");
        }
        return ret;
    }

    string LookupHost(const char* host) override {
        struct addrinfo hints, *res;
        int errcode;
        char addrstr[100];
        void* ptr = nullptr;
        vector<string> ips;
    
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags |= AI_CANONNAME;
    
        if (string(host) == string("localhost")) {
            // fix: on MacOS(ARM64) when ai_family=30 if use localhost
            errcode = getaddrinfo3(ctx.get(), "127.0.0.1", NULL, &hints, &res);
        } else {
            errcode = getaddrinfo3(ctx.get(), host, NULL, &hints, &res);
        }
        if (errcode != 0) {
            _printf("getaddrinfo failed!\n");
            return "";
        }
    
        _printf("Host: %s\n", host);
        struct addrinfo* head = res;
        while (res) {
            inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 100);
    
            switch (res->ai_family) {
                case AF_INET:
                    ptr = &((struct sockaddr_in*)res->ai_addr)->sin_addr;
                    break;
                case AF_INET6:
                    ptr = &((struct sockaddr_in6*)res->ai_addr)->sin6_addr;
                    break;
                default:
                    throw InvalidArgumentException("[LookupHost]unsupported ai_family: " + to_string(res->ai_family));
            }
            inet_ntop(res->ai_family, ptr, addrstr, 100);
            _printf("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4, addrstr,
                   res->ai_canonname);
            if (res->ai_family != PF_INET6)
                ips.push_back(string(addrstr));
            res = res->ai_next;
        }
        freeaddrinfo3(head);
    
        return ips.size() > 0? ips[0]: "";
    }

    ssize_t Send(const void* message, size_t length) override {
        return send3(ctx.get(), fd, message, length, 0);
    }

    ssize_t Recv(void* buffer, size_t length) override {
        return recv3(ctx.get(), fd, buffer, length, 0);
    }

    void Wait() override {
        WaitForBytes(ctx.get());
    }

    void SetShutdownFlag() override {
        set_shutdown_flag(ctx.get());
    }
    int GetProxySocket() override {
        return fd;
    }
    void SetProxySocket() override {
        proxySocket = fd + PROXY_SOCKET_OFFSET;
    }

    void SetWaitBytes(int wait_bytes) override {
        ctx->need_wait = true;
        ctx->wait_bytes = wait_bytes;
    }

private:
    unique_ptr<ProxyClientCtx> ctx;
    int fd = -1;
    int proxySocket = -1;
};

class SecureWebSocketProxy: public PlainWebSocketProxy {
public:
    SecureWebSocketProxy(ClientType client_type)
      : PlainWebSocketProxy(client_type),
      ssl_ctx(nullptr, SSL_CTX_free),
      ssl(nullptr, SSL_free) {}
    
    void SetSecureParameter(const char* caPath,
                            const char* host,
                            const char* cipher,
                            const char* curve) override {
        const SSL_METHOD* tlsv12 = TLS_method();
        this->ssl_ctx.reset(SSL_CTX_new(tlsv12));
        SSL_CTX* ssl_ctx = this->ssl_ctx.get();
        SSL_CTX_set_ecdh_auto(ssl_ctx, 1);
    
        // *********************************
        SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
        SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_2_VERSION);
        // int min_ver = SSL_CTX_get_min_proto_version(ssl_ctx);
        // int max_ver = SSL_CTX_get_max_proto_version(ssl_ctx);
        // _printf("min version: %d, max version: %d\n", min_ver, max_ver);
        // *********************************
    
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, verify_callback);
        #ifdef LOAD_CERT_FROM_MEM
        if (X509_STORE_load_from_memory(SSL_CTX_get_cert_store(ssl_ctx), cert_data.data()) <= 0) {
        #else
        if (SSL_CTX_load_verify_locations(ssl_ctx, NULL, caPath) <= 0) {
        #endif
            ERR_print_errors_fp(stderr);
            throw SSLCertificateError("load verify location error");
        }
    
        if (SSL_CTX_set_cipher_list(ssl_ctx, cipher) <= 0) {
            ERR_print_errors_fp(stderr);
            throw SSLCertificateError("set cipher list error");
        }
        if (SSL_CTX_set1_groups_list(ssl_ctx, curve) <= 0) {
            ERR_print_errors_fp(stderr);
            throw SSLCertificateError("set1 groups list error");
        }
    
        this->ssl.reset(SSL_new(ssl_ctx));
        SSL* ssl = this->ssl.get();
        if (host != nullptr) {
            _printf("set host name:%s\n", host);
            if (SSL_set_tlsext_host_name(ssl, host) <= 0) {
                ERR_print_errors_fp(stderr);
                throw SSLCertificateError("set tlsext host name error");
            }
        }

        SetProxySocket();
        int ret = SSL_set_fd(ssl, GetProxySocket());
        if (ret < 0) {
            _printf("ssl set fd error\n");
            throw SocketError("set fd error");
        }
    }

    int SSLConnect() override {
        return SSL_connect(ssl.get());
    }

    ssize_t SSLWrite(const void* message, size_t length) override {
        return SSL_write(ssl.get(), message, length);
    }

    ssize_t SSLRead(void* buffer, size_t length) override {
        return SSL_read(ssl.get(), buffer, length);
    }

    void SSLShutdown() override {
        SSL_shutdown(ssl.get());
    }

private:
    StdUniquePtr<SSL_CTX>::Type ssl_ctx;
    StdUniquePtr<SSL>::Type ssl;
};

struct SendResult {
    struct SendResult* next;
    void* result;
};

struct SendProxyCtx {
    bool valid = true;
    SendResult* list = nullptr;
    std::mutex mtx;
    std::condition_variable cond;
};

void PutToSendProxyCtx(SendProxyCtx* ctx, void* p);
void* GetFromSendProxyCtx(SendProxyCtx* ctx);
SendProxyCtx* NewSendProxyCtx();
void FreeSendProxyCtx(SendProxyCtx* p);
void SetSendProxyCtx(SendProxyCtx* ctx, bool valid);

struct RecvProxyCtx {
    // bool valid = true;
    StdUniquePtr<RecvCtx>::Type inner_ctx;
    uint64_t id = 0;
    int sock = -1;
    // std::mutex mtx;
    // std::condition_variable cond;

    RecvProxyCtx(): inner_ctx(nullptr, FreeRecvCtx) {}
};

RecvProxyCtx* NewRecvProxyCtx();
void FreeRecvProxyCtx(RecvProxyCtx* ctx);
void SetRecvProxyCtx(RecvProxyCtx* ctx, bool valid);

void PutToRecvProxyCtx(RecvProxyCtx* ctx, const char* buf, size_t len);
ssize_t RecvFromRecvProxyCtx(RecvProxyCtx* ctx, char* buf, size_t len);
void cancel_wait_for_call_result(ProxyClientCtx* ctx);
