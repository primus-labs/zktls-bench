#ifndef MPC_TLS_WS_HEADER_
#define MPC_TLS_WS_HEADER_

#include <string>
#include <stdio.h>
#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>
#include "common/smart_pointer.h"
#include <openssl/ssl.h>
#include "common/runtime_error.h"
#include "pado_io_channel.h"
using namespace std;

extern std::string cert_data;
int X509_STORE_load_from_memory(X509_STORE* store, const char* cert_data);

class ChannelStream;
void RequestWebSocketHandshake(ChannelStream* stream, const char* uri, const char* host);

void ResponseWebSocketHandshake(ChannelStream* stream);

void CheckWebSocketHandshake(ChannelStream* stream);

size_t GenWebSocketHeader(uint8_t* headerData, size_t numBytes, uint32_t* mask_data);

void UnMaskData(char* buf, uint64_t len, uint32_t mask);
typedef struct
#if defined(__GNUC__)
  __attribute__((packed, aligned(1)))
#endif
  WebSocketMessageHeader {
    unsigned opcode : 4;
    unsigned rsv : 3;
    unsigned fin : 1;
    unsigned payloadLength : 7;
    unsigned mask : 1;
} WebSocketMessageHeader;


#ifndef SEND_BUFFER_SIZE
#define SEND_BUFFER_SIZE 1 * 1024 * 1024
#endif

#ifndef RECV_BUFFER_SIZE
#define RECV_BUFFER_SIZE 1 * 1024 * 1024
#endif

#define ENABLE_MASK 0 

struct SendBuffer{
   private:
    char* buffer = nullptr;
    uint64_t offset;
    uint64_t send_id;
    uint64_t buffer_size;
    uint64_t packed_offset = 0;
    uint32_t mask_data;
#ifndef __EMSCRIPTEN__
    const int EXTRA_HEADER_SIZE = sizeof(WebSocketMessageHeader) + 8 + 4;
#else
    const int EXTRA_HEADER_SIZE = 0;
#endif

   public:
    SendBuffer(size_t payload_size, SendBuffer* send_buf) {
        offset = 0;
        if (send_buf != nullptr) {
            send_id = send_buf->send_id;
            send_buf->send_id++;
        } else {
            send_id = 0;
        }
        if (payload_size == 0) {
            buffer_size = SEND_BUFFER_SIZE;
        } else {
            buffer_size = EXTRA_HEADER_SIZE + 2 * sizeof(uint64_t) + payload_size;
        }
        buffer = (char*)malloc(buffer_size);
        memset(buffer, 0, buffer_size);
        offset += EXTRA_HEADER_SIZE + 2 * sizeof(uint64_t);
        mask_data = time(NULL);
    }
    SendBuffer() : SendBuffer(0, nullptr) {}

    ~SendBuffer() { free(buffer); }

    void set_send_id(SendBuffer* rhs) {
        send_id = rhs->send_id;
    }

    void pack() {
        set_send_id();
        set_length();
        set_extra_header();
    }

    bool can_put(uint64_t numBytes) { return offset + numBytes < buffer_size; }

    void put(const char* buf, uint64_t len) {
        if (can_put(len)) {
            memcpy(buffer + offset, buf, len);
            offset += len;
        }
        else {
            resize(buffer_size + (buffer_size > len? buffer_size: len));
            memcpy(buffer + offset, buf, len);
            offset += len;
        }
    }

    bool empty() { return offset <= EXTRA_HEADER_SIZE + 2 * sizeof(uint64_t); }

    size_t size() { return offset - packed_offset; }
    const char* data() { return buffer + packed_offset; }

    void reset() {
        offset = EXTRA_HEADER_SIZE + 2 * sizeof(uint64_t);
        memset(buffer, 0, offset);
    }
   private:
    void set_send_id() {
        send_id++;
        *(uint64_t*)(buffer + EXTRA_HEADER_SIZE) = send_id;
    }

    void set_length() {
        uint64_t payloadLength = get_payload_length();
        *((uint64_t*)((char*)buffer + EXTRA_HEADER_SIZE) + 1) = payloadLength;
    }

    uint64_t get_payload_length() {
        uint64_t payloadLength = offset - 2 * sizeof(uint64_t) - EXTRA_HEADER_SIZE;
        return payloadLength;
    }

    void set_extra_header() {
        if (EXTRA_HEADER_SIZE > 0) {
            uint64_t payloadLength = 2 * sizeof(uint64_t) + get_payload_length();
#if ENABLE_MASK
            size_t headerBytes = GenWebSocketHeader((uint8_t*)buffer, payloadLength, &mask_data);
            UnMaskData(buffer + EXTRA_HEADER_SIZE, payloadLength, mask_data);
#else
            size_t headerBytes = GenWebSocketHeader((uint8_t*)buffer, payloadLength, nullptr);
#endif
            packed_offset = EXTRA_HEADER_SIZE - headerBytes;
            memmove(buffer + packed_offset, buffer, headerBytes);
        }
    }

    void resize(uint64_t len) {
        buffer = (char*)realloc(buffer, len);
        buffer_size = len;
    }

};

struct FlushList {
    FlushList* next;
    char data[];
};
struct FlushBuffer {
    uint64_t length;
    char payload[];
};
struct FlushCtx {
    FlushList* list;
    std::mutex mtx;
    std::condition_variable cond;
};

FlushCtx* NewFlushCtx();
void FreeFlushCtx(FlushCtx* ctx);

void PutToFlushCtx(FlushCtx* ctx, const char* buf, size_t len);
FlushList* GetFromFlushCtx(FlushCtx* ctx);
bool EmptyFlushCtx(FlushCtx* ctx);
char* GetData(FlushList* list);
size_t GetSize(FlushList* list);

struct SendCtx {
    unique_ptr<SendBuffer> buffer;
    StdUniquePtr<FlushCtx>::Type flush_ctx;
    bool async = false;
    SendCtx() : flush_ctx(nullptr, FreeFlushCtx) {}
};

SendCtx* NewSendCtx();

void FreeSendCtx(SendCtx* ctx);

ssize_t SendMessage(SendCtx* ctx, const char* buf, size_t len, ChannelStream* stream);
ssize_t AsyncSendMessage(SendCtx* ctx, const char* buf, size_t len, ChannelStream* stream);
void AsyncFlushMessage(SendCtx* ctx, ChannelStream* stream);

struct RecvList {
    RecvList* next;
    char data[];
};
struct RecvBuffer {
    uint64_t id;
    uint64_t length;
    char payload[];
};
struct RecvInfo {
    uint64_t id;
    char* payload;
    uint64_t offset;
    uint64_t length;
    uint64_t prev_id;
    bool valid;
};

struct RecvCtx {
    RecvList* list;
    unique_ptr<RecvInfo> info;
    std::mutex mtx;
    std::condition_variable cond;

    bool recv_zero = false;
    bool is_chunk = false;
};

RecvCtx* NewRecvCtx();

void FreeRecvCtx(RecvCtx* ctx);

void PutToRecvCtx(RecvCtx* ctx, RecvList* recv_chunk);
void PutToRecvCtx(RecvCtx* ctx, const char* buf, size_t len);
void PutToRecvCtx(RecvCtx* ctx, size_t id, const char* buf, size_t len);

size_t RecvFromRecvCtx(RecvCtx* ctx, char* buf, size_t len);
RecvList* RecvFromRecvCtx(RecvCtx* ctx);

ssize_t RecvMessage(RecvCtx* ctx, char* buf, size_t len, ChannelStream* stream);
RecvList* RecvMessage(RecvCtx* ctx, uint64_t id, ChannelStream* stream);

bool EmptyRecvCtx(RecvCtx* ctx);

struct IpPort {
    enum ProtocolType {WS, WSS};

    ProtocolType protocol_type = WS;
    string ip;
    int port;
    string uri;
    string host;
    IpPort() = default;
    IpPort(const string& ip, int port, const string& protocol_name, const string& uri, const string& host) {
        this->ip = ip;
        this->port = port;
        if (protocol_name == "ws") {
            this->protocol_type = WS;
        }
        else if (protocol_name == "wss") {
            this->protocol_type = WSS;
        }
        else {
            throw InvalidArgumentException("invalid protocol name: " + protocol_name);
        }
        this->uri = uri;
        this->host = host;
    }
};
IpPort parse_url(const string& url);
int verify_callback(int ok, X509_STORE_CTX* ctx);

enum ConnectionType {
    Invalid = 0,
    CLIENT_TO_TLS_SERVER = 1,
    CLIENT_TO_PADO = 2,
};
inline void throw_connection_error(ConnectionType connection_type, string desc) {
    switch (connection_type) {
        case ConnectionType::CLIENT_TO_TLS_SERVER:
            throw TlsNetworkError(desc);
            break;
        case ConnectionType::CLIENT_TO_PADO:
            throw PrimusServerNetworkError(desc);
            break;
        default:
            throw RuntimeError(desc);
    }
}

class ChannelStream {
    public:
    virtual ssize_t Send(const void* buf, size_t nbytes) = 0;
    virtual ssize_t SendEx(const void* buf, size_t nbytes) = 0;
    virtual ssize_t Recv(void* buf, size_t nbytes) = 0;
    virtual ssize_t RecvEx(void* buf, size_t nbytes) = 0;
    virtual void Flush() {}
    virtual void Close() {}
    virtual void WaitReadyState() {}
    virtual void SetRecvTimeout(size_t timeout_msec) {}
    virtual void StopRecv() {}
    virtual ~ChannelStream() {}
    virtual void ThrowConnectionError(string desc) = 0;
};

class FileStream :public ChannelStream{
    public:
    FileStream(int fd, ConnectionType type): stream(nullptr, fclose) {
        stream.reset(fdopen(fd, "wb+"));

        buffer.reset(new char[NETWORK_BUFFER_SIZE]);
        memset(buffer.get(), 0, NETWORK_BUFFER_SIZE);
        setvbuf(stream.get(), buffer.get(), _IOFBF, NETWORK_BUFFER_SIZE);
        this->connection_type = type;
        this->fd = fd;
    }
    ssize_t Send(const void* buf, size_t nbytes) override {
        return fwrite(buf, 1, nbytes, stream.get());
    }
    ssize_t SendEx(const void* buf, size_t nbytes) override {
        return fwrite(buf, 1, nbytes, stream.get());
    }
    ssize_t Recv(void* buf, size_t nbytes) override {
        return fread(buf, 1, nbytes, stream.get());
    }
    ssize_t RecvEx(void* buf, size_t nbytes) override {
        size_t recv_bytes = 0;
        do {
            int ret = Recv((char*)buf + recv_bytes, nbytes - recv_bytes);
            if (ret <= 0){
                _printf("FileStream RecvEx ret:%d errno:%d %s nbytes:%zu\n", ret, errno, strerror(errno), nbytes);
                break;
            }
            recv_bytes += ret;
        } while (recv_bytes < nbytes);
        return recv_bytes;
    }
    void Flush() override {
        fflush(stream.get());
    }
    void Close() override {
        // fclose(stream.get());
    }
    void SetRecvTimeout(size_t timeout_msec) override {
        int timeout_sec = timeout_msec/1000;
        int timeout_usec = (timeout_msec%1000)*1000;
        struct timeval timeout = {timeout_sec, timeout_usec};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    }
    void ThrowConnectionError(string desc) override {
        throw_connection_error(connection_type, desc);
    }

    private:
    StdUniquePtr<FILE>::Type stream;
    unique_ptr<char[]> buffer;
    ConnectionType connection_type;
    int fd = -1;
};

class WebFileStream: public FileStream {
    public:
    WebFileStream(int fd, ConnectionType type): FileStream(fd, type) {
        mask_data = time(NULL);
    }

    ssize_t SendEx(const void* buf, size_t nbytes) override {
        uint8_t headerData[sizeof(WebSocketMessageHeader) + 8 + 4] =
          {};
        memset(headerData, 0, sizeof(headerData));
#if ENABLE_MASK
        size_t headerBytes = GenWebSocketHeader(headerData, nbytes, &mask_data);
        UnMaskData((char*)buf, nbytes, mask_data);
#else
        size_t headerBytes = GenWebSocketHeader(headerData, nbytes, nullptr);
#endif
        int ret = Send(headerData, headerBytes);
        if (ret != (int)headerBytes) {
            ThrowConnectionError("send message error");
            return -1;
        }

        if (Send(buf, nbytes) != (int)nbytes) {
            ThrowConnectionError("send message error");
            return -1;
        }
        return nbytes;
    }
    private:
    uint32_t mask_data;
};

class SocketStream :public ChannelStream{
    public:
    struct SocketBuffer {
        char buf[RECV_BUFFER_SIZE];
        size_t size;
        size_t offset;
        SocketBuffer() {
            size = 0;
            offset = 0;
        }
        size_t available() {
            return size - offset;
        }
        char* data() {
            return buf + offset;
        }
        void move_offset(size_t n) {
            offset += n;
        }
        void reset(size_t size) {
            this->offset = 0;
            this->size = size;
        }
    };
    SocketStream(int fd, ConnectionType type) {
        this->fd = fd;
        this->connection_type = type;
    }
    ssize_t Send(const void* buf, size_t nbytes) override {
        return ::send(fd, buf, nbytes, 0);
    }
    ssize_t SendEx(const void* buf, size_t nbytes) override {
        return ::send(fd, buf, nbytes, 0);
    }
    ssize_t Recv(void* buf, size_t nbytes) override {
        int ret = -1;
        do {
            ret = ::recv(fd, buf, nbytes, 0);
        } while (ret == -1 && (errno == EAGAIN || errno == EINTR ) && recv_again);
        return ret;
    }
    ssize_t RecvEx(void* buf, size_t nbytes) override {
        size_t recv_bytes = 0;
        do {
            size_t available = socket_buffer.available();
            int to_read_bytes = std::min(available, nbytes - recv_bytes);
            if (to_read_bytes > 0) {
                memcpy((char*)buf + recv_bytes, socket_buffer.data(), to_read_bytes);
                socket_buffer.move_offset(to_read_bytes);
                recv_bytes += to_read_bytes;
            }
            if (recv_bytes < nbytes) {
                int ret = Recv(socket_buffer.buf, RECV_BUFFER_SIZE);
                if (ret <= 0){
                    _printf("SocketStream RecvEx ret:%d errno:%d %s nbytes:%zu\n", ret, errno, strerror(errno), nbytes);
                    break;
                }
                socket_buffer.reset(ret);
            }
        } while (recv_bytes < nbytes);
        return recv_bytes;
    }
    void Close() override {
        close(fd);
    }
    void SetRecvTimeout(size_t timeout_msec) override {
        int timeout_sec = timeout_msec/1000;
        int timeout_usec = (timeout_msec%1000)*1000;
        struct timeval timeout = {timeout_sec, timeout_usec};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    }
    void StopRecv() override {
        recv_again = false;
    }
    void ThrowConnectionError(string desc) override {
        throw_connection_error(connection_type, desc);
    }

    private:
    int fd = -1;
    ConnectionType connection_type;
    SocketBuffer socket_buffer;
    bool recv_again = true;
};

class WebSocketStream: public SocketStream {
    public:
    WebSocketStream(int fd, ConnectionType type): SocketStream(fd, type) {
        mask_data = time(NULL);
    }

    ssize_t SendEx(const void* buf, size_t nbytes) override {
        uint8_t headerData[sizeof(WebSocketMessageHeader) + 8 + 4] =
          {};
        memset(headerData, 0, sizeof(headerData));
#if ENABLE_MASK
        size_t headerBytes = GenWebSocketHeader(headerData, nbytes, &mask_data);
        UnMaskData((char*)buf, nbytes, mask_data);
#else
        size_t headerBytes = GenWebSocketHeader(headerData, nbytes, nullptr);
#endif
#if 0
        int ret = Send(headerData, headerBytes);
        if (ret != (int)headerBytes) {
            ThrowConnectionError("send message error");
            return -1;
        }

        if (Send(buf, nbytes) != (int)nbytes) {
            ThrowConnectionError("send message error");
            return -1;
        }
#else
        unique_ptr<char[]> buf2(new char[headerBytes + nbytes]);
        memcpy(buf2.get(), headerData, headerBytes);
        memcpy(buf2.get() + headerBytes, buf, nbytes);
        if ((size_t)Send(buf2.get(), headerBytes + nbytes) != headerBytes + nbytes) {
            ThrowConnectionError("send message error");
            return -1;
        }
#endif
        return nbytes;
    }
    private:
    uint32_t mask_data;
};

class SSLClient {
    public:
    SSLClient(int fd, ConnectionType connection_type): ssl_ctx(nullptr, SSL_CTX_free), ssl(nullptr, SSL_free) {
        const SSL_METHOD* tlsv12 = TLS_method();
        this->ssl_ctx.reset(SSL_CTX_new(tlsv12));
        SSL_CTX* ssl_ctx = this->ssl_ctx.get();
        SSL_CTX_set_ecdh_auto(ssl_ctx, 1);
    
        // *********************************
        SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
        SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_2_VERSION);
        // *********************************
    
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, verify_callback);
        #ifdef LOAD_CERT_FROM_MEM
        if (X509_STORE_load_from_memory(SSL_CTX_get_cert_store(ssl_ctx), cert_data.data()) <= 0) {
        #else
        if (SSL_CTX_load_verify_locations(ssl_ctx, NULL, "./certs") <= 0) {
        #endif
            throw SSLCertificateError("load verify location error");
        }
    
        this->ssl.reset(SSL_new(ssl_ctx));
        SSL* ssl = this->ssl.get();

        int ret = SSL_set_fd(ssl, fd);
        if (ret < 0) {
            _printf("ssl set fd error\n");
            throw SocketError("set fd error");
        }
    
        ret = SSL_connect(ssl);
    
        if (ret < 0) {
            _printf("ssl connect error\n");
            throw_connection_error(connection_type, "ssl connect error");
        }
    }

    SSL* get() {
        return ssl.get();
    }
    private:
    StdUniquePtr<SSL_CTX>::Type ssl_ctx;
    StdUniquePtr<SSL>::Type ssl;
};

class SSLStream :public ChannelStream{
    public:
    SSLStream(int fd, ConnectionType type) {
        ssl_client.reset(new SSLClient(fd, type));
        this->ssl = ssl_client->get();
        this->connection_type = type;
    }
    ssize_t Send(const void* buf, size_t nbytes) override {
        return SSL_write(ssl, buf, nbytes);
    }
    ssize_t SendEx(const void* buf, size_t nbytes) override {
        return SSL_write(ssl, buf, nbytes);
    }
    ssize_t Recv(void* buf, size_t nbytes) override {
        return SSL_read(ssl, buf, nbytes);
    }
    ssize_t RecvEx(void* buf, size_t nbytes) override {
        size_t recv_bytes = 0;
        do {
            int ret = Recv((char*)buf + recv_bytes, nbytes - recv_bytes);
            if (ret <= 0){
                _printf("SSLStream RecvEx ret:%d errno:%d %s nbytes:%zu\n", ret, errno, strerror(errno), nbytes);
                break;
            }
            recv_bytes += ret;
        } while (recv_bytes < nbytes);
        return recv_bytes;
    }
    void Close() override {
        SSL_shutdown(ssl);
    }
    void ThrowConnectionError(string desc) override {
        throw_connection_error(connection_type, desc);
    }

    private:
    SSL* ssl = nullptr;
    unique_ptr<SSLClient> ssl_client;
    ConnectionType connection_type;
};
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/threading.h>
#include <emscripten/posix_socket.h>
typedef EMSCRIPTEN_WEBSOCKET_T (*InitWebSocketFn)(
  void* user_ctx, const char* bridgeUrl);
typedef RecvCtx* (*GetRecvCtxFn)(void* user_ctx);

EMSCRIPTEN_WEBSOCKET_T emscripten_init_websocket_to_posix_socket_bridge2(
  void* user_ctx, const char* bridgeUrl);
RecvCtx* GetRecvCtxForPado(void* user_ctx);

class EmscriptenStream :public ChannelStream{
    public:
    EmscriptenStream(void* user_ctx, const char* address, ConnectionType type, InitWebSocketFn initWebSocketFn, GetRecvCtxFn getRecvCtxFn) {
        this->user_ctx = user_ctx;
        this->wssock = initWebSocketFn(user_ctx, address);
        this->connection_type = type;
        this->getRecvCtxFn = getRecvCtxFn;
    }
    ~EmscriptenStream() {
        if (!closed) {
            emscripten_websocket_close(wssock, 0, 0);
            emscripten_websocket_delete(wssock);
        }
    }
    ssize_t Send(const void* buf, size_t nbytes) override {
        emscripten_websocket_send_binary(wssock, (void*)buf, nbytes);
        return nbytes;
    }
    ssize_t SendEx(const void* buf, size_t nbytes) override {
        emscripten_websocket_send_binary(wssock, (void*)buf, nbytes);
        return nbytes;
    }
    ssize_t Recv(void* buf, size_t nbytes) override {
        assert(connection_type != ConnectionType::CLIENT_TO_TLS_SERVER);
        RecvCtx* ctx = getRecvCtxFn(user_ctx);
        size_t recv_bytes = 0;
        while (recv_bytes < nbytes) {
            std::unique_lock<mutex> lck(ctx->mtx);
    
            while (!ctx->recv_zero && (ctx->info == nullptr || !ctx->info->valid)) {
                ctx->cond.wait(lck);
            }
    
            if (ctx->info == nullptr || !ctx->info->valid) {
                string server = connection_type == ConnectionType::CLIENT_TO_TLS_SERVER? "proxy": "pado";
                string errmsg = "recv from  " + server + " error";
                throw NetworkError(errmsg);
            }
    
            size_t ret = RecvFromRecvCtx(ctx, (char*)buf + recv_bytes, nbytes - recv_bytes);
            recv_bytes += ret;
        }
    
        return recv_bytes;
    }
    ssize_t RecvEx(void* buf, size_t nbytes) override {
        size_t recv_bytes = 0;
        do {
            int ret = Recv((char*)buf + recv_bytes, nbytes - recv_bytes);
            if (ret <= 0){
                _printf("SocketStream RecvEx ret:%d errno:%d nbytes:%zu\n", ret, errno, nbytes);
                break;
            }
            recv_bytes += ret;
        } while (recv_bytes < nbytes);
        return recv_bytes;
    }
    void Close() override {
        emscripten_websocket_close(wssock, 0, 0);
        emscripten_websocket_delete(wssock);
        closed = true;
    }
    void WaitReadyState() override {
        // Synchronously wait until connection has been established.
        RecvCtx* ctx = getRecvCtxFn(user_ctx);
        uint16_t readyState = 0;
        do {
            emscripten_websocket_get_ready_state(wssock, &readyState);
            emscripten_thread_sleep(100);
        } while (readyState == 0);
        if(ctx->recv_zero){
            string server = connection_type == ConnectionType::CLIENT_TO_TLS_SERVER? "proxy": "pado";
            string errmsg = "connect to " + server + " error";
            throw_connection_error(connection_type, errmsg);
        }
    }
    void ThrowConnectionError(string desc) override {
        throw_connection_error(connection_type, desc);
    }

    private:
    int wssock = -1;
    bool closed = false;
    void* user_ctx = nullptr;
    GetRecvCtxFn getRecvCtxFn = nullptr;
    ConnectionType connection_type;
};

#endif

#endif
