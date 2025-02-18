#include "backend/backend.h"
#include "emp-zk/emp-zk.h"
#include <iostream>
#include "emp-tool/emp-tool.h"
#include "protocol/handshake.h"
#include "protocol/record.h"
#include "protocol/com_conv.h"
#include "protocol/aead.h"
#include "protocol/aead_izk.h"
#include "protocol/post_record.h"
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

const int threads = 4;

template <typename IO>
void full_protocol(IO* io, IO* io_opt, COT<IO>* cot, int party) {
    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
    HandShake<PrimusIO>* hs = new HandShake<PrimusIO>(io, io_opt, cot, group);

    EC_POINT* V = EC_POINT_new(group);
    EC_POINT* Tc = EC_POINT_new(group);
    BIGNUM* t = BN_new();

    BIGNUM* ts = BN_new();
    EC_POINT* Ts = EC_POINT_new(hs->group);
    BN_set_word(ts, 2);
    EC_POINT_mul(hs->group, Ts, ts, NULL, NULL, hs->ctx);

    unsigned char* rc = new unsigned char[32];
    unsigned char* rs = new unsigned char[32];

    unsigned char* ufinc = new unsigned char[finished_msg_length];
    unsigned char* ufins = new unsigned char[finished_msg_length];

    unsigned char* tau_c = new unsigned char[32];
    unsigned char* tau_s = new unsigned char[32];

    unsigned char* cmsg = new unsigned char[QUERY_BYTE_LEN];
    unsigned char* smsg = new unsigned char[RESPONSE_BYTE_LEN];

    memset(rc, 0x11, 32);
    memset(rs, 0x22, 32);
    memset(tau_c, 0x33, 32);
    memset(tau_s, 0x44, 32);
    memset(cmsg, 0x55, QUERY_BYTE_LEN);
    memset(smsg, 0x66, RESPONSE_BYTE_LEN);

    unsigned char aad[] = {0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed,
                           0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef, 0xab, 0xad, 0xda, 0xd2};

    size_t aad_len = sizeof(aad);
    if (party == BOB) {
        hs->compute_primus_VA(V, Ts);
    } else {
        hs->compute_client_VB(Tc, V, Ts);
    }

    hs->compute_pms_offline(party);

    BIGNUM* pms = BN_new();
    BIGNUM* full_pms = BN_new();
    hs->compute_pms_online(pms, V, party);

    //hs->compute_master_key(pms, rc, 32, rs, 32);

    // Use session_hash instead of rc!
    hs->compute_extended_master_key(pms, rc, 32);

    hs->compute_expansion_keys(rc, 32, rs, 32);

    hs->compute_client_finished_msg(client_finished_label, client_finished_label_length, tau_c,
                                    32);
    hs->compute_server_finished_msg(server_finished_label, server_finished_label_length, tau_s,
                                    32);

    // padding the last 8 bytes of iv_c and iv_s according to TLS!
    unsigned char iv_c_oct[8], iv_s_oct[8];
    memset(iv_c_oct, 0x11, 8);
    memset(iv_s_oct, 0x22, 8);
    AEAD<IO>* aead_c =
      new AEAD<IO>(io, io_opt, cot, hs->client_write_key, hs->client_write_iv);
    AEAD<IO>* aead_s =
      new AEAD<IO>(io, io_opt, cot, hs->server_write_key, hs->server_write_iv);

    Record<IO>* rd = new Record<IO>;

    unsigned char* finc_ctxt = new unsigned char[finished_msg_length];
    unsigned char* finc_tag = new unsigned char[tag_length];
    unsigned char* msg = new unsigned char[finished_msg_length];

    // Use correct message instead of hs->client_ufin!
    hs->encrypt_client_finished_msg(aead_c, finc_ctxt, finc_tag, hs->client_ufin, 12, aad,
                                    aad_len, iv_c_oct, 8, party);

    // Use correct ciphertext instead of finc_ctxt!
    hs->decrypt_server_finished_msg(aead_s, msg, finc_ctxt, finished_msg_length, finc_tag, aad,
                                    aad_len, iv_s_oct, 8, party);

    unsigned char* cctxt = new unsigned char[QUERY_BYTE_LEN];
    unsigned char* ctag = new unsigned char[tag_length];

    unsigned char* sctxt = new unsigned char[RESPONSE_BYTE_LEN];
    unsigned char* stag = new unsigned char[tag_length];

    // the client encrypts the first message, and sends to the server.
    rd->encrypt(aead_c, io, cctxt, ctag, cmsg, QUERY_BYTE_LEN, aad, aad_len, iv_c_oct, 8,
                party);
    // prove handshake in post-record phase.
    switch_to_zk();
    PostRecord<IO>* prd = new PostRecord<IO>(io, hs, aead_c, aead_s, rd, party);
    prd->reveal_pms(Ts);
    // Use correct finc_ctxt, fins_ctxt, iv_c, iv_s according to TLS!
    prd->prove_and_check_handshake_step1(rc, 32, rs, 32, tau_c, 32, tau_s, 32, rc, 32, true);
    prd->prove_and_check_handshake_step2(finc_ctxt, finished_msg_length, iv_c_oct, 8);
    prd->prove_and_check_handshake_step3(finc_ctxt, finished_msg_length, iv_s_oct, 8);
    Integer prd_cmsg, prd_cmsg2, prd_smsg, prd_smsg2, prd_cz0, prd_c2z0, prd_sz0, prd_s2z0;
    prd->prove_record_client(prd_cmsg, prd_cz0, cctxt, QUERY_BYTE_LEN, iv_c_oct, 8);
    prd->prove_record_server_last(prd_smsg2, prd_s2z0, cctxt, RESPONSE_BYTE_LEN, iv_s_oct, 8);

    // Use correct finc_ctxt and fins_ctxt!
    prd->finalize_check(finc_ctxt, finc_tag, 12, aad, finc_ctxt, finc_tag, 12, aad, {prd_cz0},
                        {cctxt}, {ctag}, {QUERY_BYTE_LEN}, {aad}, 1, {prd_sz0}, {sctxt},
                        {stag}, {RESPONSE_BYTE_LEN}, {aad}, 1, aad_len);

    sync_zk_gc<IO>();
    switch_to_gc();
    EC_POINT_free(V);
    EC_POINT_free(Tc);
    BN_free(t);
    BN_free(ts);
    BN_free(pms);
    BN_free(full_pms);
    EC_POINT_free(Ts);

    delete hs;
    delete[] rc;
    delete[] rs;
    delete[] ufinc;
    delete[] ufins;
    delete[] tau_c;
    delete[] tau_s;
    delete[] finc_ctxt;
    delete[] finc_tag;
    delete[] msg;
    delete[] cmsg;
    delete[] smsg;

    delete aead_c;
    delete aead_s;
    delete rd;
    delete prd;
    EC_GROUP_free(group);
}

string test_protocol(const string& args) {
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
    PrimusIO* io_opt = createPrimusIO(party == ALICE, ip, port + threads);

    BoolIO<PrimusIO>* ios[threads];
    PrimusIO* io[threads];
    for (int i = 0; i < threads; i++) {
        io[i] = createPrimusIO(party == ALICE, ip, port + i);
        ios[i] = new BoolIO<PrimusIO>(io[i], party == ALICE);
    }
    PrimusIO* io0 = io[0];

    auto start = emp::clock_start();
    setup_protocol<PrimusIO>(io0, ios, threads, party);
    auto prot = (PrimusParty<PrimusIO>*)(ProtocolExecution::prot_exec);
    IKNP<PrimusIO>* cot = prot->ot;
    full_protocol<PrimusIO>(io0, io_opt, cot, party);

    finalize_protocol();

    bool cheat = CheatRecord::cheated();
    if (cheat)
        error("cheat!\n");

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
    totalCounter += io_opt->counter;
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

    for (int i = 0; i < threads; i++) {
        delete ios[i];
        delete io[i];
    }
    delete io_opt;
    return j2.dump();
}
