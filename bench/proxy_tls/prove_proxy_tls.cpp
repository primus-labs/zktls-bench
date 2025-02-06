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

using namespace std;
using namespace emp;

bool prove_proxy_tls(int party) {
    bool res = false;
    unsigned char pms_buf[] = {0xe4, 0x2b, 0xf7, 0x1c, 0x75, 0x85, 0x21, 0x79, 0x57, 0xe7, 0x48, 0x37, 0xd8, 0xb9, 0x1a, 0xda, 0xce, 0x31, 0x1b, 0x48, 0x4d, 0xfb, 0x5c, 0x1c, 0x65, 0x10, 0x10, 0xb0, 0x77, 0x64, 0x27, 0x7f};
    unsigned char client_random[] = {0xdc, 0x98, 0x3a, 0xbe, 0xd8, 0x1f, 0x90, 0xb4, 0x23, 0xb8, 0x3b, 0xc1, 0x0c, 0xfb, 0xf6, 0xe4, 0x6d, 0xe7, 0xd6, 0xcf, 0x13, 0x4a, 0xf2, 0x5f, 0x63, 0xb5, 0x3d, 0x69, 0x7d, 0x2e, 0x04, 0x45};
    unsigned char server_random[] = {0x9a, 0x31, 0x1b, 0x7e, 0x66, 0x86, 0xc0, 0x1d, 0xb8, 0xea, 0x00, 0x73, 0xf4, 0x0d, 0x0b, 0x19, 0x49, 0x19, 0xaa, 0xb9, 0xcb, 0x99, 0x0e, 0xdb, 0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x01};
    unsigned char master_secret_hash[] = {0xc8, 0x6e, 0xc1, 0xce, 0x45, 0x01, 0x30, 0xf4, 0x80, 0xf6, 0xe9, 0x9a, 0x96, 0x6e, 0xa4, 0x40, 0x71, 0x43, 0x83, 0xff, 0x85, 0x25, 0xc4, 0x49, 0x57, 0xff, 0x2e, 0x3c, 0x43, 0x17, 0x43, 0x58};
    unsigned char client_finish_hash[] = {0xc8, 0x6e, 0xc1, 0xce, 0x45, 0x01, 0x30, 0xf4, 0x80, 0xf6, 0xe9, 0x9a, 0x96, 0x6e, 0xa4, 0x40, 0x71, 0x43, 0x83, 0xff, 0x85, 0x25, 0xc4, 0x49, 0x57, 0xff, 0x2e, 0x3c, 0x43, 0x17, 0x43, 0x58};
    unsigned char server_finish_hash[] = {0x96, 0x20, 0xb0, 0x3d, 0xcc, 0x9a, 0x04, 0x56, 0xf4, 0xaf, 0xcb, 0xe0, 0x87, 0x28, 0x2f, 0x7d, 0x4d, 0x7a, 0xf4, 0x73, 0x17, 0x3c, 0xab, 0xcb, 0x6e, 0x8a, 0xa6, 0x35, 0x75, 0x9f, 0x7a, 0xab};
    unsigned char client_fin_iv[] = {0xa1, 0x60, 0x62, 0x7c, 0x3c, 0xc3, 0x9f, 0x8e};
    unsigned char client_fin_ctxt[] = {0xf3, 0x3e, 0x33, 0xb7, 0xad, 0xf3, 0x0a, 0xbd, 0x49, 0xa4, 0xdc, 0x2d, 0x7b, 0x00, 0xe1, 0x05};
    unsigned char server_fin_iv[] =  {0xd9, 0x2f, 0xd9, 0xa4, 0xa1, 0x38, 0x1b, 0xca};
    unsigned char server_fin_ctxt[] = {0x02, 0x42, 0x44, 0x7a, 0xc3, 0x3d, 0x4b, 0xf9, 0x89, 0x0e, 0x00, 0xd9, 0x7d, 0x06, 0x87, 0xb5};

    unsigned char http_msg[] = {0x47, 0x45, 0x54, 0x20, 0x2f, 0x61, 0x70, 0x69, 0x2f, 0x76, 0x31, 0x2f, 0x67, 0x65, 0x74, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x31, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0x0d, 0x0a, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, 0x0d, 0x0a, 0x0d, 0x0a};
    unsigned char http_iv[] = {0xa1, 0x60, 0x62, 0x7c, 0x3c, 0xc3, 0x9f, 0x8f};
    unsigned char http_ctxt[] = {0x9b, 0x75, 0xf9, 0x00, 0x0a, 0xd3, 0x9d, 0xef, 0x92, 0x56, 0x4a, 0x78, 0xa1, 0xb4, 0xa0, 0x71, 0xe5, 0x8c, 0xd7, 0x07, 0x61, 0xef, 0xb2, 0x4e, 0x28, 0xe1, 0x40, 0xa2, 0x72, 0xf6, 0xad, 0xa7, 0xbf, 0x6d, 0xe6, 0x0f, 0x65, 0xe7, 0xe8, 0xc1, 0x98, 0xad, 0xcd, 0x37, 0x8e, 0xbd, 0x19, 0xa8, 0x05, 0xbf, 0x52};

    // prove prf
    Integer key_c, key_s, iv_c, iv_s, fin_c, fin_s;
    {
        Integer ms;
        PRFProver prover;

        BIGNUM* pms = BN_new();
        if (party == ALICE) {
            BN_bin2bn(pms_buf, sizeof(pms_buf), pms);
        }
        prover.prove_extended_master_key(ms, pms, master_secret_hash, sizeof(master_secret_hash), party);
        prover.prove_expansion_keys(key_c, key_s, iv_c, iv_s, ms, client_random, sizeof(client_random), server_random, sizeof(server_random), party); 
        prover.prove_client_finished_msg(fin_c, ms, client_finish_hash, sizeof(client_finish_hash), party);
        prover.prove_server_finished_msg(fin_s, ms, server_finish_hash, sizeof(server_finish_hash), party); 

        unsigned char fin_head[] = {0x14, 0x00, 0x00, 0x0c};
        reverse(fin_head, fin_head + sizeof(fin_head));
        Integer tmp(32, fin_head, PUBLIC);

        fin_c.bits.insert(fin_c.bits.end(), tmp.bits.begin(), tmp.bits.end());
        fin_s.bits.insert(fin_s.bits.end(), tmp.bits.begin(), tmp.bits.end());

        BN_free(pms);
    }

    // prove aes
    {
        AESProver prover_c(key_c, iv_c);
        AESProver prover_s(key_s, iv_s);

        res = prover_c.prove_private_msgs(client_fin_iv, sizeof(client_fin_iv), fin_c, client_fin_ctxt, sizeof(client_fin_ctxt));
        if (!res) {
            error("prove client finish msg error");
        }

        res = prover_s.prove_private_msgs(server_fin_iv, sizeof(server_fin_iv), fin_s, server_fin_ctxt, sizeof(server_fin_ctxt));
        if (!res) {
            error("prove server finish msg error");
        }

        unsigned char* buf = new unsigned char[sizeof(http_msg)];
        memcpy(buf, http_msg, sizeof(http_msg));
        reverse(buf, buf + sizeof(http_msg));
        Integer tmp(8 * sizeof(http_msg), buf, ALICE);

        res = prover_c.prove_private_msgs(http_iv, sizeof(http_iv), tmp, http_ctxt, sizeof(http_ctxt));
        if (!res) {
            error("prove http msg error");
        }
        delete[] buf;
         
    }
    return res;
}

const int threads = 1;
int main(int argc, char** argv) {
    int port, party;
    parse_party_and_port(argv, &party, &port);
    NetIO* io[threads];
    for (int i = 0; i < threads; i++) {
        io[i] = new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i);
    }
    BoolIO<NetIO>* ios[threads];
    for (int i = 0; i < threads; i++)
        ios[i] = new BoolIO<NetIO>(io[i], party == ALICE);

    auto start = emp::clock_start();
    auto comm = io[0]->counter;

    setup_proxy_protocol(ios, threads, party);

    cout << "setup time: " << emp::time_from(start) << " us" << endl;
    cout << "setup comm: " << (io[0]->counter - comm) * 1.0 / 1024 << " Kbytes" << endl;

    comm = io[0]->counter;

    start = emp::clock_start();
    bool res = prove_proxy_tls(party);
    if (!res) {
        error("prove error:\n");
    }

    cout << "zk AND gates: " << CircuitExecution::circ_exec->num_and() << endl;

    bool cheated = finalize_proxy_protocol<BoolIO<NetIO>>();
    if (cheated)
        error("cheated\n");

    cout << "prove time: " << emp::time_from(start) << " us" << endl;
    cout << "prove comm: " << (io[0]->counter - comm) * 1.0 / 1024 << " Kbytes" << endl;
#if defined(__linux__)
    struct rusage rusage;
    if (!getrusage(RUSAGE_SELF, &rusage))
        std::cout << "[Linux]Peak resident set size: " << (size_t)rusage.ru_maxrss
                  << std::endl;
    else
        std::cout << "[Linux]Query RSS failed" << std::endl;
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) ==
        KERN_SUCCESS)
        std::cout << "[Mac]Peak resident set size: " << (size_t)info.resident_size_max
                  << std::endl;
    else
        std::cout << "[Mac]Query RSS failed" << std::endl;
#endif

    for (int i = 0; i < threads; ++i) {
        delete ios[i]->io;
        delete ios[i];
    }

    return 0;
}
