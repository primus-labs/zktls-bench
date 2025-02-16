#pragma once
#include <string>
#include <stdint.h>
using namespace std;

void init_openssl();
 
enum class ClientType {
    MPC = 1,
    PLAIN = 2,
};

class WebSocketProxy {
public:
    virtual ~WebSocketProxy() {}
    virtual ClientType GetClientType() = 0; 

    virtual void Init(const char* bridgeUrl) = 0;

    virtual int Socket() = 0;

    virtual int Connect(const char* ip, int port) = 0;

    virtual string LookupHost(const char* host) = 0;

    virtual ssize_t Send(const void* message, size_t length) = 0;

    virtual ssize_t Recv(void* buffer, size_t length) = 0; 

    virtual void Wait() = 0;

    virtual void SetShutdownFlag() = 0;

    virtual int GetProxySocket() = 0;

    virtual void SetProxySocket() = 0; 

    virtual void SetWaitBytes(int wait_bytes) = 0;

    virtual void SetSecureParameter(const char* caPath,
                                    const char* host,
                                    const char* cipher,
                                    const char* curve) {
    }

    virtual int SSLConnect() {
        return 0;
    }

    virtual ssize_t SSLWrite(const void* message, size_t length) {
        return 0;
    }

    virtual ssize_t SSLRead(void* buffer, size_t length) {
        return 0;
    }

    virtual void SSLShutdown() {
    }
};

WebSocketProxy* NewPlainWebSocketProxy(ClientType clientType);
WebSocketProxy* NewSecureWebSocketProxy(ClientType clientType);
