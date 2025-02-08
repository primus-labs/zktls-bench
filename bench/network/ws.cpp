#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <netdb.h>
#include <vector>
#include <map>
#include <string>
#include "sha1.h"
#include "ws.h"
using namespace std;
// #include "mpc_tls.h"
#include "common/smart_pointer.h"

uint64_t ntoh64(uint64_t x);
#define hton64 ntoh64

// thread-safe, re-entrant
uint64_t ntoh64(uint64_t x) {
    return ntohl(x >> 32) | ((uint64_t)ntohl(x & 0xFFFFFFFFu) << 32);
}
  static const
  unsigned char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_encode(void* dst, const void* src, size_t len) // thread-safe, re-entrant
{
    assert(dst != src);
    unsigned int* d = (unsigned int*)dst;
    const unsigned char* s = (const unsigned char*)src;
    const unsigned char* end = s + len;
    while (s < end) {
        uint32_t e = *s++ << 16;
        if (s < end)
            e |= *s++ << 8;
        if (s < end)
            e |= *s++;
        *d++ = b64[e >> 18] | (b64[(e >> 12) & 0x3F] << 8) | (b64[(e >> 6) & 0x3F] << 16) |
               (b64[e & 0x3F] << 24);
    }
    for (size_t i = 0; i < (3 - (len % 3)) % 3; i++)
        ((char*)d)[-1 - i] = '=';
}

#define BUFFER_SIZE 1024
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

// Given a multiline string of HTTP headers, returns a pointer to the beginning of the value of given header inside the string that was passed in.
static int GetHttpHeader(const char* headers,
                         const char* header,
                         char* out,
                         int maxBytesOut) // thread-safe, re-entrant
{
    const char* pos = strstr(headers, header);
    if (!pos) {
        throw NetworkError("Sec-WebSocket-Key not find");
    }
    pos += strlen(header);
    const char* end = pos;
    while (*end != '\r' && *end != '\n' && *end != '\0')
        ++end;
    int numBytesToWrite = MIN((int)(end - pos), maxBytesOut - 1);
    memcpy(out, pos, numBytesToWrite);
    out[numBytesToWrite] = '\0';
    return (int)(end - pos);
}

// Sends WebSocket handshake back to the given WebSocket connection.
void SendHandshake(ChannelStream* stream, const char* request) {
    const char webSocketGlobalGuid[] =
      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // 36 characters long
    char key[128 + sizeof(webSocketGlobalGuid)];
    GetHttpHeader(request, "Sec-WebSocket-Key: ", key, sizeof(key) / 2);
    strcat(key, webSocketGlobalGuid);

    char sha1[21];
    // _printf("hashing key: \"%s\"\n", key);
    SHA1(sha1, key, (int)strlen(key));

    char handshakeMsg[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: 0000000000000000000000000000\r\n"
      "\r\n";

    base64_encode(
      strstr(handshakeMsg, "Sec-WebSocket-Accept: ") + strlen("Sec-WebSocket-Accept: "), sha1,
      20);

    int err = stream->Send(handshakeMsg, (int)strlen(handshakeMsg));
    if (err < 0) {
        stream->Close();
        stream->ThrowConnectionError("send websocket handshake error");
    }
    // _printf("Sent handshake[%d]:\n%s\n", strlen(handshakeMsg), handshakeMsg);
}

// Validates if the given, possibly partially received WebSocket message has enough bytes to contain a full WebSocket header.
static bool WebSocketHasFullHeader(uint8_t* data, uint64_t obtainedNumBytes) {
    if (obtainedNumBytes < 2)
        return false;
    uint64_t expectedNumBytes = 2;
    WebSocketMessageHeader* header = (WebSocketMessageHeader*)data;
    if (header->mask)
        expectedNumBytes += 4;
    switch (header->payloadLength) {
        case 127:
            return expectedNumBytes += 8;
            break;
        case 126:
            return expectedNumBytes += 2;
            break;
        default:
            break;
    }
    return obtainedNumBytes >= expectedNumBytes;
}

// Computes the total number of bytes that the given WebSocket message will take up.
static uint64_t WebSocketFullMessageSize(uint8_t* data, uint64_t obtainedNumBytes) {
    assert(WebSocketHasFullHeader(data, obtainedNumBytes));

    uint64_t expectedNumBytes = 2;
    WebSocketMessageHeader* header = (WebSocketMessageHeader*)data;
    if (header->mask)
        expectedNumBytes += 4;
    switch (header->payloadLength) {
        case 127:
            return expectedNumBytes += 8 + ntoh64(*(uint64_t*)(data + 2));
            break;
        case 126:
            return expectedNumBytes += 2 + ntohs(*(uint16_t*)(data + 2));
            break;
        default:
            expectedNumBytes += header->payloadLength;
            break;
    }
    return expectedNumBytes;
}

// Tests the structure integrity of the websocket message length.
static bool WebSocketValidateMessageSize(uint8_t* data, uint64_t obtainedNumBytes) {
    uint64_t expectedNumBytes = WebSocketFullMessageSize(data, obtainedNumBytes);

    if (expectedNumBytes != obtainedNumBytes) {
        _printf("Corrupt WebSocket message size! (got %llu bytes, expected %llu bytes)\n",
               (unsigned long long)obtainedNumBytes, (unsigned long long)expectedNumBytes);
        _printf("Received data:");
        for (size_t i = 0; i < obtainedNumBytes; ++i)
            _printf(" %02X", data[i]);
        _printf("\n");
    }
    return expectedNumBytes == obtainedNumBytes;
}

static uint64_t WebSocketMessagePayloadLength(uint8_t* data, uint64_t numBytes) {
    WebSocketMessageHeader* header = (WebSocketMessageHeader*)data;
    switch (header->payloadLength) {
        case 127:
            return ntoh64(*(uint64_t*)(data + 2));
        case 126:
            return ntohs(*(uint16_t*)(data + 2));
        default:
            return header->payloadLength;
    }
}

static uint32_t WebSocketMessageMaskingKey(uint8_t* data, uint64_t numBytes) {
    WebSocketMessageHeader* header = (WebSocketMessageHeader*)data;
    if (!header->mask)
        return 0;
    switch (header->payloadLength) {
        case 127:
            return *(uint32_t*)(data + 10);
        case 126:
            return *(uint32_t*)(data + 4);
        default:
            return *(uint32_t*)(data + 2);
    }
}

static uint8_t* WebSocketMessageData(uint8_t* data, uint64_t numBytes) {
    WebSocketMessageHeader* header = (WebSocketMessageHeader*)data;
    data += 2;     // Two bytes of fixed size header
    if (header->mask)
        data += 4; // If there is a masking key present in the header, that takes up 4 bytes
    switch (header->payloadLength) {
        case 127:
            return data + 8; // 64-bit length
        case 126:
            return data + 2; // 16-bit length
        default:
            return data;     // 7-bit length that was embedded in fixed size header.
    }
}

static void CloseWebSocket(int client_fd) {
    _printf("Closing WebSocket connection %d\n", client_fd);
    // CloseAllSocketsByConnection(client_fd);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
}

static const char* WebSocketOpcodeToString(int opcode) {
    static const char* opcodes[] = {"continuation frame (0x0)",
                                    "text frame (0x1)",
                                    "binary frame (0x2)",
                                    "reserved(0x3)",
                                    "reserved(0x4)",
                                    "reserved(0x5)",
                                    "reserved(0x6)",
                                    "reserved(0x7)",
                                    "connection close (0x8)",
                                    "ping (0x9)",
                                    "pong (0xA)",
                                    "reserved(0xB)",
                                    "reserved(0xC)",
                                    "reserved(0xD)",
                                    "reserved(0xE)",
                                    "reserved(0xF)"};
    return opcodes[opcode];
}

// thread-safe, re-entrant
static void WebSocketMessageUnmaskPayload(uint8_t* payload,
                                   uint64_t payloadLength,
                                   uint32_t maskingKey) {
    uint8_t maskingKey8[4];
    memcpy(maskingKey8, &maskingKey, 4);
    uint32_t* data_u32 = (uint32_t*)payload;
    uint32_t* end_u32 = (uint32_t*)((uintptr_t)(payload + (payloadLength & ~3u)));

    while (data_u32 < end_u32)
        *data_u32++ ^= maskingKey;

    uint8_t* end = payload + payloadLength;
    uint8_t* data = (uint8_t*)data_u32;
    while (data < end) {
        *data ^= maskingKey8[(data - payload) % 4];
        ++data;
    }
}

void RequestWebSocketHandshake(ChannelStream* stream, const char* uri, const char* host) {
    char handshakeMsgFormat[] =
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/110.0\r\n"
      "Accept: */*\r\n"
      "Accept-Language: en-US,en;q=0.5\r\n"
      "Accept-Encoding: gzip, deflate, br\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Sec-WebSocket-Extensions: permessage-deflate\r\n"
      "Sec-WebSocket-Key: I28w97h3i3dkMdxGpUF+sA==\r\n"
      "Connection: keep-alive, Upgrade\r\n"
      "Sec-Fetch-Dest: websocket\r\n"
      "Sec-Fetch-Mode: websocket\r\n"
      "Sec-Fetch-Site: same-site\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Upgrade: websocket\r\n"
      "\r\n";
    char handshakeMsg[1024];
    snprintf(handshakeMsg, sizeof(handshakeMsg), handshakeMsgFormat, uri, host);
    // _printf("request handshake:%s", handshakeMsg);
    int ret = stream->Send(handshakeMsg, strlen(handshakeMsg));
    if (ret < 0) {
        stream->Close();
        stream->ThrowConnectionError("request websocket send error");
    }
}

void ResponseWebSocketHandshake(ChannelStream* stream) {
    // Waiting for connection upgrade handshake
    char buf[BUFFER_SIZE];
    int read = stream->Recv(buf, BUFFER_SIZE);

    if (!read) {
        _printf("peer close\n");
        stream->Close();
        stream->ThrowConnectionError("response websocket peer close");
    }

    if (read < 0) {
        fprintf(stderr, "Client read failed2 %d %s\n", errno, strerror(errno));
        stream->Close();
        stream->ThrowConnectionError("response websocket recv error");
    }

    SendHandshake(stream, buf);

}

void CheckWebSocketHandshake(ChannelStream* stream) {
    char buf[BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));
    int read = stream->Recv(buf, BUFFER_SIZE);
    if (read <= 0) {
        _printf("check websocket handshake:%d %s\n", errno, strerror(errno));
        stream->Close();
        stream->ThrowConnectionError("check websocket recv error");
    }

    // _printf("check websocket handshake[%d]:%s\n", strlen(buf), buf);
}

SendCtx* NewSendCtx() {
    SendCtx* ctx = new SendCtx();
    // ujnss: set send buffer to 32K

    ctx->buffer.reset(new SendBuffer(32 * 1024, nullptr));
    ctx->flush_ctx.reset(new FlushCtx());
    return ctx;
}

void FreeSendCtx(SendCtx* ctx) {
    delete ctx;
}

size_t GenWebSocketHeader(uint8_t* headerData, size_t numBytes, uint32_t* mask_data) {
    WebSocketMessageHeader* header = (WebSocketMessageHeader*)headerData;
    header->opcode = 0x02;
    header->fin = 1;
    int headerBytes = 2;

    if (numBytes < 126) {
        header->payloadLength = numBytes;
    } else if (numBytes <= 65535) {
        header->payloadLength = 126;
        *(uint16_t*)(headerData + headerBytes) = htons((unsigned short)numBytes);
        headerBytes += 2;
    } else {
        header->payloadLength = 127;
        *(uint64_t*)(headerData + headerBytes) = hton64(numBytes);
        headerBytes += 8;
    }
    if (mask_data != nullptr) {
        header->mask = 1;
        *(uint32_t*)(headerData + headerBytes) = *mask_data;
        headerBytes += 4;
    }
    return headerBytes;
}

ssize_t SendMessage(SendCtx* ctx, const char* buf, size_t len, ChannelStream* stream) {
    SendBuffer* send_buffer = ctx->buffer.get();

    if (len > 0 && send_buffer->can_put(len)) {
        send_buffer->put(buf, len);
        return len;
    }

    if (!send_buffer->empty()) {
        send_buffer->pack();
        if (stream->SendEx(send_buffer->data(), send_buffer->size()) < 0) {
            return -1;
        }

        send_buffer->reset();
    }

    if (len > 0 && send_buffer->can_put(len)) {
        send_buffer->put(buf, len);
        return len;
    } else if (len > 0) {
        SendBuffer tmp(len, send_buffer);

        tmp.put(buf, len);
        tmp.pack();
        if (stream->SendEx(tmp.data(), tmp.size()) < 0) {
            return -1;
        }
    }
    return len;
}

ssize_t AsyncSendMessage(SendCtx* ctx, const char* buf, size_t len, ChannelStream* stream) {
    SendBuffer* send_buffer = ctx->buffer.get();
 
    if (len == 0) {
        if (!send_buffer->empty()) {
            send_buffer->pack();
            PutToFlushCtx(ctx->flush_ctx.get(), send_buffer->data(), send_buffer->size());
            send_buffer->reset();
        }
        return len;
    }

    if (len > 0 && send_buffer->can_put(len)) {
        send_buffer->put(buf, len);
        return len;
    }

    if (!send_buffer->empty()) {
        send_buffer->pack();
        PutToFlushCtx(ctx->flush_ctx.get(), send_buffer->data(), send_buffer->size());
        send_buffer->reset();
    }

    if (len > 0 && send_buffer->can_put(len)) {
        send_buffer->put(buf, len);
        return len;
    } else if (len > 0) {
        SendBuffer tmp(len, send_buffer);

        tmp.put(buf, len);
        tmp.pack();
        PutToFlushCtx(ctx->flush_ctx.get(), tmp.data(), tmp.size());
    }
    return len;
}

void AsyncFlushMessage(SendCtx* ctx, ChannelStream* stream) {
    FlushCtx* flush_ctx = ctx->flush_ctx.get();
 
    unique_lock<std::mutex> lck(flush_ctx->mtx);
    while (ctx->async && EmptyFlushCtx(flush_ctx)) {
        flush_ctx->cond.wait(lck);
    }
    FlushList* list = GetFromFlushCtx(flush_ctx);
    if (list != nullptr) {
        FlushBuffer* flush_buffer = (FlushBuffer*)list->data;
        if (stream->SendEx(flush_buffer->payload, flush_buffer->length) < 0) {
            free(list);
            return;
        }

        free(list);
        stream->Flush();
    }

}

RecvCtx* NewRecvCtx() {
    RecvCtx* ctx = new RecvCtx();
    ctx->list = nullptr;
    return ctx;
}

void FreeRecvCtx(RecvCtx* ctx) {
    RecvList *p = ctx->list;
    while (p) {
        RecvList *next = p->next;
        free(p);
        p = next;
    }
    delete ctx;
}

void UnMaskData(char* buf, uint64_t len, uint32_t mask) {
    uint32_t* begin32 = (uint32_t*)buf;
    uint32_t* end32 = begin32 + len / sizeof(uint32_t);
    while (begin32 != end32) {
        *begin32++ ^= mask;
    }

    uint8_t* begin8 = (uint8_t*)end32;
    uint8_t* end8 = (uint8_t*)(buf + len);
    uint8_t* mask8 = (uint8_t*)&mask;
    while (begin8 != end8) {
        *begin8++ ^= *mask8++;
    }
}

void PrintRecvCtx(RecvCtx* ctx) {
    RecvList* p = ctx->list;
    _printf("recv ctx list: ");
    while (p != nullptr) {
        RecvBuffer* b = (RecvBuffer*)p->data;
        _printf("%llu:%llu-> ", (unsigned long long)b->id, (unsigned long long)b->length);
        p = p->next;
    }
    _printf("\n");
}

void PutToRecvCtx(RecvCtx* ctx, RecvList* recv_chunk) {
    RecvBuffer* recv_buffer = (RecvBuffer*)recv_chunk->data;
    if (ctx->list == nullptr) {
        ctx->list = recv_chunk;
    } else {
        RecvList* p_list = nullptr;
        RecvList* q_list = ctx->list;
        while (q_list != nullptr) {
            RecvBuffer* b = (RecvBuffer*)q_list->data;

            if (recv_buffer->id < b->id) {
                recv_chunk->next = q_list;

                if (p_list == nullptr) {
                    ctx->list = recv_chunk;
                } else {
                    p_list->next = recv_chunk;
                }
                break;
            }

            p_list = q_list;
            q_list = q_list->next;
        }

        if (q_list == nullptr) {
            p_list->next = recv_chunk;
        }
    }
    if (ctx->is_chunk) return;

    if (ctx->info == nullptr) {
        ctx->info.reset(new RecvInfo);
        ctx->info->valid = false;
        ctx->info->prev_id = 0;
    }
    if (!ctx->info->valid && ctx->list != nullptr) {
        RecvBuffer* b = (RecvBuffer*)ctx->list->data;
        if (ctx->info->prev_id + 1 == b->id) {
            ctx->info->id = b->id;
            ctx->info->length = b->length;
            ctx->info->payload = b->payload;
            ctx->info->offset = 0;
            ctx->info->valid = true;
        }
    }
}

void PutToRecvCtx(RecvCtx* ctx, const char* buf, size_t len) {
    RecvList* recv_chunk = (RecvList*)malloc(sizeof(RecvList) + len);
    recv_chunk->next = nullptr;
    memcpy(recv_chunk->data, buf, len);
    PutToRecvCtx(ctx, recv_chunk);
}
void PutToRecvCtx(RecvCtx* ctx, size_t id, const char* buf, size_t len) {
    RecvList* recv_chunk = (RecvList*)malloc(sizeof(RecvList) + sizeof(RecvBuffer) + len);
    recv_chunk->next = nullptr;

    RecvBuffer* b = (RecvBuffer*)recv_chunk->data;
    b->id = id == (size_t)-1? *(int*)buf: id;
    b->length = len;
    memcpy(b->payload, buf, len);
    PutToRecvCtx(ctx, recv_chunk);
}
bool EmptyRecvCtx(RecvCtx* ctx) {
    if (ctx->is_chunk) {
        return ctx->list == nullptr;
    }
    if (ctx->list == nullptr || ctx->info == nullptr) {
        return true;
    }
    if (ctx->info->valid && ctx->info->offset < ctx->info->length) {
        return false;
    }
    return true;
}
size_t RecvFromRecvCtx(RecvCtx* ctx, char* buf, size_t len) {
    size_t recv_bytes = 0;
    if (ctx->info->offset < ctx->info->length) {
        uint64_t min_len =
          min(ctx->info->length - ctx->info->offset, (uint64_t)(len - recv_bytes));
        memcpy(buf + recv_bytes, ctx->info->payload + ctx->info->offset, min_len);
        ctx->info->offset += min_len;

        recv_bytes += min_len;

        if (ctx->info->offset == ctx->info->length) {
            RecvList* tmp = ctx->list;
            ctx->list = ctx->list->next;

            RecvBuffer* p = (RecvBuffer*)tmp->data;

            ctx->info->prev_id = p->id;
            ctx->info->valid = false;

            free(tmp);
        }
    }
    if (!ctx->info->valid && ctx->list != nullptr) {
        RecvBuffer* b = (RecvBuffer*)ctx->list->data;
        if (ctx->info->prev_id + 1 == b->id) {
            ctx->info->id = b->id;
            ctx->info->length = b->length;
            ctx->info->payload = b->payload;
            ctx->info->offset = 0;
            ctx->info->valid = true;
        }
    }
    return recv_bytes;
}
RecvList* RecvFromRecvCtx(RecvCtx* ctx, uint64_t id) {
    RecvList* p = ctx->list;
    if (p) {
        RecvBuffer* recv_buffer = (RecvBuffer*)p->data;
        if (recv_buffer->id == id) {
            ctx->list = p->next;
        }
        else if (id < recv_buffer->id) {
            return nullptr;
        }
        else {
            RecvList* next = p->next;
            while (next) {
                RecvBuffer* buffer = (RecvBuffer*)next->data;
                if (buffer->id == id) {
                    p->next = next->next;
                    return next;
                }
                else if (id < buffer->id) {
                    return nullptr;
                }
                p = next;
                next = p->next;
            }
            return nullptr;
        }
    }
    return p;
}

FlushCtx* NewFlushCtx() {
    FlushCtx* ctx = new FlushCtx();
    ctx->list = nullptr;
    return ctx;
}
void FreeFlushCtx(FlushCtx* ctx) {
    FlushList* p = ctx->list;
    while (p) {
        FlushList* next = p->next;
        free(p);
        p = next;
    }
    delete ctx;
}

void PutToFlushCtx(FlushCtx* ctx, const char* buf, size_t len) {
    FlushList *flush_chunk = (FlushList*)malloc(sizeof(FlushList) + sizeof(FlushBuffer) + len);
    flush_chunk->next = nullptr;

    FlushBuffer* flush_buffer = (FlushBuffer*)flush_chunk->data;
    flush_buffer->length = len;
    memcpy(flush_buffer->payload, buf, len);

    unique_lock<std::mutex> lck(ctx->mtx);
    if (ctx->list == nullptr) {
        ctx->list = flush_chunk;
    }
    else {
        FlushList *p = ctx->list;
        while (p->next) {
            p = p->next;
        }
        p->next = flush_chunk;
    }
    ctx->cond.notify_one();
}
FlushList* GetFromFlushCtx(FlushCtx* ctx) {
    FlushList* tmp = ctx->list;
    if (tmp) {
        ctx->list = tmp->next;
    }
    return tmp;
}
bool EmptyFlushCtx(FlushCtx* ctx) {
    return ctx->list == nullptr;
}
char* GetData(FlushList* list) {
    FlushBuffer* buffer = (FlushBuffer*)list->data;
    return buffer->payload;
}

size_t GetSize(FlushList* list) {
    FlushBuffer* buffer = (FlushBuffer*)list->data;
    return buffer->length;
}
ssize_t RecvMessageHeader(uint64_t &data_len, bool &is_masked, uint32_t &mask_data, ChannelStream* stream) {
    char headerData[sizeof(WebSocketMessageHeader) + 8 + 4];
    int ret = stream->RecvEx(headerData, 2);
    if (ret != 2) {
        _printf("ret:%d expect0:2\n", ret);
        fflush(stdout);
        stream->ThrowConnectionError("recv websocket header error");
        return -1;
    }
    WebSocketMessageHeader* header = (WebSocketMessageHeader*)headerData;
    is_masked = header->mask ? true : false;
    uint8_t payload_length = header->payloadLength;
    uint64_t mask_length = is_masked ? 4 : 0;

    uint64_t length_size;
    switch (payload_length) {
        case 126:
            length_size = 2;
            break;
        case 127:
            length_size = 8;
            break;
        default:
            length_size = 0;
    }

    mask_data = 0;
    int mask_offset = 2;
    data_len = payload_length;
    if (length_size + mask_length > 0) {
        if (stream->RecvEx(headerData + 2, length_size + mask_length) !=
            (int)(length_size + mask_length)) {
            stream->ThrowConnectionError("recv websocket header extra data error");
            return -1;
        }
        switch (payload_length) {
            case 126:
                data_len = ntohs(*(uint16_t*)(headerData + 2));
                mask_offset += 2;
                break;
            case 127:
                data_len = ntoh64(*(uint64_t*)(headerData + 2));
                mask_offset += 8;
                break;
        }

        if (is_masked) {
            mask_data = *(uint32_t*)(headerData + mask_offset);
        }
    }
    return 0;

}
RecvList* RecvMessage(RecvCtx* ctx, uint64_t id, ChannelStream* stream) {
    RecvList* res = nullptr;
    while (1) {
        res = RecvFromRecvCtx(ctx, id);
        if (res) {
            break;
        }

        uint64_t data_len = 0;
        bool mask = false;
        uint32_t mask_data = 0;
        int ret = RecvMessageHeader(data_len, mask, mask_data, stream);
        if (ret < 0) {
            _printf("ret: %d errno:%d  %s\n", ret, errno, strerror(errno));
            fflush(stdout);
            stream->ThrowConnectionError("recv websocket header error");
            return nullptr;
        }

        RecvList* recv_chunk = (RecvList*)malloc(sizeof(RecvList) + sizeof(RecvBuffer) + data_len);
        RecvBuffer* recv_buffer = (RecvBuffer*)recv_chunk->data;

        ret = stream->RecvEx(recv_buffer->payload, data_len);
        if (mask) {
            UnMaskData(recv_buffer->payload, data_len, mask_data);
        }
        if (ret != (int)data_len) {
            _printf("ret: %d expect2: %llu\n", ret, (unsigned long long)data_len);
            fflush(stdout);
            stream->ThrowConnectionError("recv websocket payload error");
            return nullptr;
        }
        recv_chunk->next = nullptr;
        recv_buffer->id = *(int*)recv_buffer->payload;
        recv_buffer->length = data_len;

        PutToRecvCtx(ctx, recv_chunk);
    }
    return res;
}
ssize_t RecvMessage(RecvCtx* ctx, char* buf, size_t len, ChannelStream* stream) {
#ifdef __EMSCRIPTEN__
    size_t recv_bytes = 0;
    while (recv_bytes < len) {
        std::unique_lock<mutex> lck(ctx->mtx);

        while (!ctx->recv_zero && (ctx->info == nullptr || !ctx->info->valid)) {
            ctx->cond.wait(lck);
        }

        if (ctx->info == nullptr || !ctx->info->valid) {
            throw PrimusServerNetworkError("recv error");
        }

        size_t ret = RecvFromRecvCtx(ctx, buf + recv_bytes, len - recv_bytes);
        recv_bytes += ret;
    }

    return recv_bytes;
#else
    uint64_t recv_bytes = 0;
    while (1) {
        if (ctx->info != nullptr && ctx->info->valid) {
            size_t ret = RecvFromRecvCtx(ctx, buf + recv_bytes, len - recv_bytes);

            recv_bytes += ret;
            if (recv_bytes == len)
                return len;
        }
        
        uint64_t data_len = 0;
        bool mask = false;
        uint32_t mask_data = 0;
        int ret = RecvMessageHeader(data_len, mask, mask_data, stream);
        if (ret < 0) {
            _printf("ret: %d errno:%d  %s\n", ret, errno, strerror(errno));
            fflush(stdout);
            stream->ThrowConnectionError("recv websocket header error");
            return -1;
        }

        RecvList* recv_chunk = (RecvList*)malloc(sizeof(RecvList) + data_len);
        ret = stream->RecvEx(recv_chunk->data, data_len);
        if (mask) {
            UnMaskData(recv_chunk->data, data_len, mask_data);
        }
        if (ret != (int)data_len) {
            _printf("ret: %d expect2: %llu\n", ret, (unsigned long long)data_len);
            fflush(stdout);
            stream->ThrowConnectionError("recv websocket payload error");
            return -1;
        }
        recv_chunk->next = nullptr;

        PutToRecvCtx(ctx, recv_chunk);
    }
    return 0;
#endif
}
static vector<string> lookup_host(const char* host) {
    struct addrinfo hints, *res;
    int errcode;
    char addrstr[100];
    void* ptr = nullptr;
    vector<string> ips;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo(host, NULL, &hints, &res);
    if (errcode != 0) {
        _printf("getaddrinfo failed!\n");
        return ips;
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
                throw InvalidArgumentException("[lookup_host]unsupported ai_family: " + to_string(res->ai_family));
        }
        inet_ntop(res->ai_family, ptr, addrstr, 100);
        _printf("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4, addrstr,
               res->ai_canonname);
        if (res->ai_family != PF_INET6)
            ips.push_back(string(addrstr));
        res = res->ai_next;
    }
    freeaddrinfo(head);

    return ips;
}

IpPort parse_url(const string& url) {
    string begin_splitter = "://";
    string end_splitter = "/";
    auto begin = url.find(begin_splitter);
    if (begin == string::npos) {
        throw InvalidArgumentException("invalid url: " + url);
    }
    string protocol_name = url.substr(0, begin);
    string uri = "/";
    
    auto end = url.find(end_splitter, begin + begin_splitter.size());
    if (end == string::npos) {
        end = url.size();
    }
    else {
        uri = url.substr(end, url.size() - end);
    }

    string address = url.substr(begin + begin_splitter.size(), end - (begin + begin_splitter.size()));
    int port = 443;
    auto p = address.find(":");
    if (p != string::npos) {
        port = atoi(address.substr(p + 1, address.size() - (p + 1)).c_str());
        address = address.substr(0, p);
    }

    auto not_ip = [](const string& address) {
        for (size_t i = 0; i < address.size(); i++) {
            if (address[i] != '.' && !(address[i] >= '0' && address[i] <= '9'))
                return true;
        }
        return false;
    };
    string host = address;

    if (not_ip(address)) {
        vector<string> addresses = lookup_host(address.c_str());
        if (addresses.empty()) {
            throw SocketError("lookup host error, url: " + url);
        }
        address = addresses[0];
    }
    
    return IpPort(address, port, protocol_name, host, uri);
}

int verify_callback(int ok, X509_STORE_CTX* ctx) {
    if (!ok) {
        int err = X509_STORE_CTX_get_error(ctx);
        char buf[1024];
        snprintf(buf, sizeof(buf), "verify callback %s\n", X509_verify_cert_error_string(err));
        throw SSLCertificateError(string(buf));
    }
    X509* cert = X509_STORE_CTX_get_current_cert(ctx);
    GENERAL_NAMES* general_names =
      (GENERAL_NAMES*)X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
    STACK_PTR_TYPE_OF(GENERAL_NAME) general_names_ptr(general_names, SK_DELETER(GENERAL_NAME));
    int count = sk_GENERAL_NAME_num(general_names);
    for (int i = 0; i < count; i++) {
        GENERAL_NAME* generalName = sk_GENERAL_NAME_value(general_names, i);
        const unsigned char* dns =
          ASN1_STRING_data((ASN1_STRING*)GENERAL_NAME_get0_value(generalName, NULL));
        _printf("dns => %s\n", dns);
    }
    return ok;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/threading.h>
#include <emscripten/posix_socket.h>


static EM_BOOL bridge_socket_on_message2(int eventType,
                                         const EmscriptenWebSocketMessageEvent* websocketEvent,
                                         void* userData) {
    RecvCtx* ctx = (RecvCtx*)userData;

    std::unique_lock<mutex> lck(ctx->mtx);
    PutToRecvCtx(ctx, (const char*)websocketEvent->data, websocketEvent->numBytes);

    ctx->cond.notify_one();

    return EM_TRUE;
}

static EM_BOOL bridge_socket_on_error2(int eventType,
                                       const EmscriptenWebSocketErrorEvent* websocketEvent,
                                       void* userData) {
    RecvCtx* ctx = (RecvCtx*)userData;
    unique_lock<std::mutex> lck(ctx->mtx);
    ctx->recv_zero = true;
    ctx->cond.notify_one();
    _printf("connect to pado error\n");
    fflush(stdout);
    return EM_TRUE;
}
static EM_BOOL bridge_socket_on_open2(int eventType,
                                      const EmscriptenWebSocketOpenEvent* websocketEvent,
                                      void* userData) {
    _printf("connect to pado successfully\n");
    fflush(stdout);
    return EM_TRUE;
}
static EM_BOOL bridge_socket_on_close2(int eventType,
                                       const EmscriptenWebSocketCloseEvent* websocketEvent,
                                       void* userData) {
    RecvCtx* ctx = (RecvCtx*)userData;
    unique_lock<std::mutex> lck(ctx->mtx);
    ctx->recv_zero = true;
    ctx->cond.notify_one();
    _printf("pado close connection, code:%d, reason:%s\n", websocketEvent->code, websocketEvent->reason);
    fflush(stdout);
    return EM_TRUE;
}
EMSCRIPTEN_WEBSOCKET_T emscripten_init_websocket_to_posix_socket_bridge2(
  void* user_ctx, const char* bridgeUrl) {
    RecvCtx* ctx = (RecvCtx*)user_ctx;
    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = bridgeUrl;
    int webSocket = emscripten_websocket_new(&attr);
    emscripten_websocket_set_onmessage_callback_on_thread(
      webSocket, ctx, bridge_socket_on_message2,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
    emscripten_websocket_set_onerror_callback_on_thread(
      webSocket, ctx, bridge_socket_on_error2,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
    emscripten_websocket_set_onopen_callback_on_thread(
      webSocket, ctx, bridge_socket_on_open2,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
    emscripten_websocket_set_onclose_callback_on_thread(
      webSocket, ctx, bridge_socket_on_close2,
      EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
    return webSocket;
}
RecvCtx* GetRecvCtxForPado(void* user_ctx) {
    return (RecvCtx*)user_ctx;
}
#endif
