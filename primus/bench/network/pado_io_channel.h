#ifndef _PADO_IO_CHANNEL_
#define _PADO_IO_CHANNEL_

#include "emp-tool/io/io_channel.h"
#include <memory>
using namespace emp;

template<class IO>
class PadoIOChannel: public IOChannel<IO> {
    public:
    using IOChannel<IO>::send_data;
    using IOChannel<IO>::recv_data;
    public:
        template<class T, int N = sizeof(T)>
        void send_size(const T* size) {
            T s = *size;
            std::unique_ptr<unsigned char[]> cPtr(new unsigned char[N]);
            unsigned char* c = cPtr.get();
            for (int i = 0; i < N; i++)
                c[i] = s >> ((N - 1 - i) << 3) & 0xff;
            send_data((const void*)c, (size_t)N);
        }

        template<class T, int N = sizeof(T)>
        void recv_size(T* size) {
            T s = 0;
            unsigned char* c = (unsigned char*)size;
            recv_data((void *)c, (size_t)N);

            for (int i = 0; i < N; i++)
                s |= (T)c[i] << ((N - 1 - i) << 3);
            *size = s;
        }
};

#endif
