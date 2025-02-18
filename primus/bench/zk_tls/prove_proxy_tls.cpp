#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>
#include "cipher/utils.h"
#include "cipher/prf.h"
#include "protocol/prove_aes.h"
#include "protocol/prove_prf.h"
#if defined(__linux__)
#include <sys/time.h>
#include <sys/resource.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <sys/resource.h>
#include <mach/mach.h>
#endif
#include "websocket_io_channel.h"
#include <json.hpp>
using json = nlohmann::ordered_json;
#include "bench.h"

using namespace std;
using namespace emp;

// http request size
static size_t QUERY_BYTE_LEN = 2 * 1024;
// http response size
static size_t RESPONSE_BYTE_LEN = 2 * 1024;

// aes gcm local encryption function
static void aes_gcm_encrypt(unsigned char* out,
                            unsigned char* gcm_tag,
                            size_t tag_len,
                            const unsigned char* gcm_key,
                            const unsigned char* gcm_iv,
                            const unsigned char* gcm_aad,
                            size_t aad_len,
                            const unsigned char* gcm_pt,
                            size_t pt_len) {
    int outlen;
    unique_ptr<unsigned char[]> outbuf_ptr(new unsigned char[pt_len + aad_len + tag_len]);
    unsigned char* outbuf = outbuf_ptr.get();
    StdUniquePtr<EVP_CIPHER_CTX>::Type ctx_ptr(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    EVP_CIPHER_CTX* ctx = ctx_ptr.get();
    /* Set cipher type and mode */
    EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
    /* Set IV length if default 96 bits is not appropriate */
    // EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, sizeof(gcm_iv), NULL);
    /* Initialise key and IV */
    EVP_EncryptInit_ex(ctx, NULL, NULL, gcm_key, gcm_iv);
    /* Zero or more calls to specify any AAD */
    EVP_EncryptUpdate(ctx, NULL, &outlen, gcm_aad, aad_len);
    /* Encrypt plaintext */
    EVP_EncryptUpdate(ctx, out, &outlen, gcm_pt, pt_len);
    /* Finalise: note get no output for GCM */
    EVP_EncryptFinal_ex(ctx, outbuf, &outlen);
    /* Get tag */
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag_len, gcm_tag);
    /* Output tag */
    //EVP_CIPHER_CTX_free(ctx);
}

#define TLS_KEY_SIZE 16
#define TLS_IV_SIZE 12
#define TLS_FIXED_IV_SIZE 4
#define TLS_TAG_SIZE 16
#define TLS_AAD_SIZE 13

// prove prf and aes in proxy-tls model
bool prove_proxy_tls(int party) {
    bool res = false;
    // premaster secret
    unsigned char pms_buf[] = {0xe4, 0x2b, 0xf7, 0x1c, 0x75, 0x85, 0x21, 0x79,
                               0x57, 0xe7, 0x48, 0x37, 0xd8, 0xb9, 0x1a, 0xda,
                               0xce, 0x31, 0x1b, 0x48, 0x4d, 0xfb, 0x5c, 0x1c,
                               0x65, 0x10, 0x10, 0xb0, 0x77, 0x64, 0x27, 0x7f};
    // client random
    unsigned char client_random[] = {0xdc, 0x98, 0x3a, 0xbe, 0xd8, 0x1f, 0x90, 0xb4,
                                     0x23, 0xb8, 0x3b, 0xc1, 0x0c, 0xfb, 0xf6, 0xe4,
                                     0x6d, 0xe7, 0xd6, 0xcf, 0x13, 0x4a, 0xf2, 0x5f,
                                     0x63, 0xb5, 0x3d, 0x69, 0x7d, 0x2e, 0x04, 0x45};
    // server random
    unsigned char server_random[] = {0x9a, 0x31, 0x1b, 0x7e, 0x66, 0x86, 0xc0, 0x1d,
                                     0xb8, 0xea, 0x00, 0x73, 0xf4, 0x0d, 0x0b, 0x19,
                                     0x49, 0x19, 0xaa, 0xb9, 0xcb, 0x99, 0x0e, 0xdb,
                                     0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x01};
    // handshake session hash for computing master secret
    unsigned char master_secret_hash[] = {0xc8, 0x6e, 0xc1, 0xce, 0x45, 0x01, 0x30, 0xf4,
                                          0x80, 0xf6, 0xe9, 0x9a, 0x96, 0x6e, 0xa4, 0x40,
                                          0x71, 0x43, 0x83, 0xff, 0x85, 0x25, 0xc4, 0x49,
                                          0x57, 0xff, 0x2e, 0x3c, 0x43, 0x17, 0x43, 0x58};
    // handshake session hash for computing client finish msg
    unsigned char client_finish_hash[] = {0xc8, 0x6e, 0xc1, 0xce, 0x45, 0x01, 0x30, 0xf4,
                                          0x80, 0xf6, 0xe9, 0x9a, 0x96, 0x6e, 0xa4, 0x40,
                                          0x71, 0x43, 0x83, 0xff, 0x85, 0x25, 0xc4, 0x49,
                                          0x57, 0xff, 0x2e, 0x3c, 0x43, 0x17, 0x43, 0x58};
    // handshake session hash for computing server finish msg
    unsigned char server_finish_hash[] = {0x96, 0x20, 0xb0, 0x3d, 0xcc, 0x9a, 0x04, 0x56,
                                          0xf4, 0xaf, 0xcb, 0xe0, 0x87, 0x28, 0x2f, 0x7d,
                                          0x4d, 0x7a, 0xf4, 0x73, 0x17, 0x3c, 0xab, 0xcb,
                                          0x6e, 0x8a, 0xa6, 0x35, 0x75, 0x9f, 0x7a, 0xab};
    // iv for encrypting client finish msg
    unsigned char client_fin_iv[] = {0xa1, 0x60, 0x62, 0x7c, 0x3c, 0xc3, 0x9f, 0x8e};
    // client finish ciphertext
    unsigned char client_fin_ctxt[] = {0xf3, 0x3e, 0x33, 0xb7, 0xad, 0xf3, 0x0a, 0xbd,
                                       0x49, 0xa4, 0xdc, 0x2d, 0x7b, 0x00, 0xe1, 0x05};
    // iv for encrypting server finish msg
    unsigned char server_fin_iv[] = {0xd9, 0x2f, 0xd9, 0xa4, 0xa1, 0x38, 0x1b, 0xca};
    // server finish ciphertext
    unsigned char server_fin_ctxt[] = {0x02, 0x42, 0x44, 0x7a, 0xc3, 0x3d, 0x4b, 0xf9,
                                       0x89, 0x0e, 0x00, 0xd9, 0x7d, 0x06, 0x87, 0xb5};

    // iv for encrypting/decrypting http response
    unsigned char http_iv[] = {0xa1, 0x60, 0x62, 0x7c, 0x3c, 0xc3, 0x9f, 0x8f};
    // http response plaintext
    unsigned char* http_msg_resp = new unsigned char[RESPONSE_BYTE_LEN];
    // http response ciphertext
    unsigned char* http_ctxt_resp = new unsigned char[RESPONSE_BYTE_LEN];
    // tag for encrypting/decrypting http response
    unsigned char http_resp_tag[TLS_TAG_SIZE];
    // aad for encrypting/decrypting http response
    unsigned char http_resp_aad[TLS_AAD_SIZE];

    memset(http_msg_resp, 0x00, RESPONSE_BYTE_LEN);
    memset(http_ctxt_resp, 0x00, RESPONSE_BYTE_LEN);
    memset(http_resp_tag, 0x00, TLS_TAG_SIZE);
    memset(http_resp_aad, 0x00, TLS_AAD_SIZE);

    // prove prf
    Integer key_c, key_s, iv_c, iv_s, fin_c, fin_s;
    {
        Integer ms;
        PRFProver prover;

        BIGNUM* pms = BN_new();
        if (party == ALICE) {
            BN_bin2bn(pms_buf, sizeof(pms_buf), pms);
        }
        // prove extended master secret
        prover.prove_extended_master_key(ms, pms, master_secret_hash,
                                         sizeof(master_secret_hash), party);
        // prove expansion keys
        prover.prove_expansion_keys(key_c, key_s, iv_c, iv_s, ms, client_random,
                                    sizeof(client_random), server_random,
                                    sizeof(server_random), party);
        // prove client finish msg
        prover.prove_client_finished_msg(fin_c, ms, client_finish_hash,
                                         sizeof(client_finish_hash), party);
        // prove server finish msg
        prover.prove_server_finished_msg(fin_s, ms, server_finish_hash,
                                         sizeof(server_finish_hash), party);

        unsigned char fin_head[] = {0x14, 0x00, 0x00, 0x0c};
        reverse(fin_head, fin_head + sizeof(fin_head));
        Integer tmp(32, fin_head, PUBLIC);

        fin_c.bits.insert(fin_c.bits.end(), tmp.bits.begin(), tmp.bits.end());
        fin_s.bits.insert(fin_s.bits.end(), tmp.bits.begin(), tmp.bits.end());

        BN_free(pms);
    }

    // generate http_ctxt_resp according to http_msg_resp
    {
        unsigned char key[TLS_KEY_SIZE];
        unsigned char iv[TLS_IV_SIZE];

        key_s.reveal<unsigned char>(key, PUBLIC);
        reverse(key, key + TLS_KEY_SIZE);

        iv_s.reveal<unsigned char>(iv, PUBLIC);
        reverse(iv, iv + TLS_FIXED_IV_SIZE);
        memcpy(iv + TLS_FIXED_IV_SIZE, http_iv, TLS_IV_SIZE - TLS_FIXED_IV_SIZE);
        aes_gcm_encrypt(http_ctxt_resp, http_resp_tag, TLS_TAG_SIZE, key, iv, http_resp_aad,
                        TLS_AAD_SIZE, http_msg_resp, RESPONSE_BYTE_LEN);
    }

    // prove aes
    {
        // aes prover for client encryption
        AESProver prover_c(key_c, iv_c);
        // aes prover for server encryption
        AESProver prover_s(key_s, iv_s);

        // prove encrypting client finish msg
        res = prover_c.prove_private_msgs(client_fin_iv, sizeof(client_fin_iv), fin_c,
                                          client_fin_ctxt, sizeof(client_fin_ctxt));
        if (!res) {
            error("prove client finish msg error");
        }

        // prove decrypting server finish ciphertext
        res = prover_s.prove_private_msgs(server_fin_iv, sizeof(server_fin_iv), fin_s,
                                          server_fin_ctxt, sizeof(server_fin_ctxt));
        if (!res) {
            error("prove server finish msg error");
        }

        // prove http response
        {
            unsigned char* buf = new unsigned char[RESPONSE_BYTE_LEN];
            memcpy(buf, http_msg_resp, RESPONSE_BYTE_LEN);
            reverse(buf, buf + RESPONSE_BYTE_LEN);
            Integer tmp(8 * RESPONSE_BYTE_LEN, buf, ALICE);

            // prove decryption http response
            res = prover_s.prove_private_msgs(http_iv, sizeof(http_iv), tmp, http_ctxt_resp,
                                              RESPONSE_BYTE_LEN);
            if (!res) {
                error("prove http msg error");
            }
            delete[] buf;
        }
    }
    return res;
}

const int threads = 4;
string test_prove_proxy_tls(const string& args) {
    json j = json::parse(args);
    string partyStr = j["party"];
    string portStr = j["port"];
    string requestSizeStr = j["requestSize"];
    string responseSizeStr = j["responseSize"];
    string ip = j["ip"];

    int party = atoi(partyStr.c_str());
    int port = atoi(portStr.c_str());
    QUERY_BYTE_LEN = atoi(requestSizeStr.c_str());
    RESPONSE_BYTE_LEN = atoi(responseSizeStr.c_str());
    PrimusIO* io[threads];
    BoolIO<PrimusIO>* ios[threads];
    for (int i = 0; i < threads; i++) {
        io[i] = createPrimusIO(party == ALICE, ip, port + i);
        ios[i] = new BoolIO<PrimusIO>(io[i], party == ALICE);
    }

    auto start = emp::clock_start();

    setup_proxy_protocol(ios, threads, party);

    bool res = prove_proxy_tls(party);
    if (!res) {
        error("prove error:\n");
    }

    bool cheated = finalize_proxy_protocol<BoolIO<PrimusIO>>();
    if (cheated)
        error("cheated\n");

    size_t memory = 0;
#if defined(__linux__)
    struct rusage rusage;
    if (!getrusage(RUSAGE_SELF, &rusage)) {
        std::cout << "[Linux]Peak resident set size: " << (size_t)rusage.ru_maxrss
                  << std::endl;
        memory = rusage.ru_maxrss;
    } else
        std::cout << "[Linux]Query RSS failed" << std::endl;
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) ==
        KERN_SUCCESS) {
        std::cout << "[Mac]Peak resident set size: " << (size_t)info.resident_size_max
                  << std::endl;
        memory = info.resident_size_max;
    } else
        std::cout << "[Mac]Query RSS failed" << std::endl;
#endif

    size_t totalCounter = 0;
    // sum up send bytes of all io
    for (int i = 0; i < threads; i++) {
        totalCounter += io[i]->counter;
    }
    uint32_t recvCounter = totalCounter;
    // sync send bytes of verifier to prover
    if (party == ALICE) {
        io[0]->recv_data(&recvCounter, sizeof(recvCounter));
    } else {
        io[0]->send_data(&recvCounter, sizeof(recvCounter));
        io[0]->flush();
        recvCounter = 0;
    }

    json j2 = {{"requestSize", QUERY_BYTE_LEN},
               {"responseSize", RESPONSE_BYTE_LEN},
               {"sendBytes", totalCounter},
               {"recvBytes", recvCounter},
               {"totalCost", emp::time_from(start) / 1e3},
               {"memory", memory}};
    for (int i = 0; i < threads; ++i) {
        delete ios[i];
        delete io[i];
    }

    return j2.dump();
}
