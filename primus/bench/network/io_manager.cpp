#include "network/network.h"

#define ASYNC_FLUSH 1 

#define PRIVATE_PORT_START 5000
#define PORT_MAP_SIZE 16
#define MAX_PORT_SEQ (PORT_MAP_SIZE * sizeof(uint64_t) * 8)
static uint64_t port_map[PORT_MAP_SIZE] = {0};
static int port_seq = 1;
static std::mutex port_mtx;

static int get_unused_port() {
    std::unique_lock<std::mutex> lck(port_mtx);
    int port = -1;
    do {
        int i = port_seq / (8 * sizeof(uint64_t));
        int j = port_seq % (8 * sizeof(uint64_t));
        if (!(port_map[i] & (1ULL << j))) {
            port_map[i] |= (1ULL << j);
            port = port_seq;
        }
        port_seq++;
        if (port_seq == MAX_PORT_SEQ)
            port_seq = 1;
    } while (port == -1);

    return port;
}

static void free_used_port(int port) {
    std::unique_lock<std::mutex> lck(port_mtx);
    int i = port / (8 * sizeof(uint64_t));
    int j = port % (8 * sizeof(uint64_t));
    port_map[i] &= ~(1ULL << j);
}

static int get_pado_io_num() {
#if REUSE_IO
    int num = threads > 2 ? threads : 2;
#else
    int num = threads + 2;
#endif
    return num;
}

#define MAX_PADO_IO_COUNT 65536
#define CHECK_IO_INTERVAL 600

#define EXCEPTION_COUNT 1000
#define EXCEPTION_DURATION 60

class IOManager {
    private:
    std::map<uint32_t, vector<PadoIO*>> pado_io_map;
    std::map<uint32_t, uint32_t> created_timestamp_map;
    uint32_t previous_check_timestamp = 0;

    std::mutex io_map_mtx;
    std::condition_variable io_map_cond;
    bool listen_flag = true;
    std::mutex exception_mtx;
    vector<uint32_t> exception_timestamp;

    public:
    IOManager() = default;

    bool check_exception_frequency() {
        std::unique_lock<std::mutex> lck(exception_mtx);
        uint32_t current_time = time(NULL);
        exception_timestamp.push_back(current_time);
    
        uint32_t exception_size = exception_timestamp.size();
        if (exception_size >= EXCEPTION_COUNT) {
            // exit if EXCEPTION_COUNT exceptions happened in EXCEPTION_DURATION seconds
            if (exception_timestamp[exception_size - EXCEPTION_COUNT] - current_time <
                EXCEPTION_DURATION) {
                printf("too much exception\n");
                fflush(stdout);
                return true;
            } else if (exception_size >=
                       EXCEPTION_COUNT *
                         2) { // truncate if 2 * EXCEPTION_COUNT exceptions happened
                auto begin_iter = exception_timestamp.begin();
                exception_timestamp.erase(begin_iter, begin_iter + EXCEPTION_COUNT);
            }
        }
        return false;
    }

    void clear_pado_io(bool force) {
        uint32_t timestamp = time(NULL);
        if (force || timestamp - previous_check_timestamp > CHECK_IO_INTERVAL) {
            std::unique_lock<std::mutex> lck(io_map_mtx);
    
            for (auto iter = created_timestamp_map.begin(); iter != created_timestamp_map.end();) {
                if (force || timestamp - iter->second > CHECK_IO_INTERVAL) {
                    uint32_t number = iter->first;
                    created_timestamp_map.erase(iter++);
                    auto io_iter = pado_io_map.find(number);
    
                    if (io_iter != pado_io_map.end()) {
                        vector<PadoIO*> vec = io_iter->second;
    
                        for (size_t i = 0; i < vec.size(); i++) {
                            delete vec[i];
                        }
                        io_iter->second.clear();
                        pado_io_map.erase(io_iter);
                    }
                } else {
                    ++iter;
                }
            }
            previous_check_timestamp = timestamp;
        }
    }

    void stop_listen() { listen_flag = false; }

    void init_pado_io(unique_ptr<PadoIO> io) {
        uint32_t number;
        uint32_t timestamp = time(NULL);
        uint8_t has_number;
        size_t num = get_pado_io_num();
        try {
            io->Init();
            io->recv_size(&has_number);
            if (has_number) {
                io->recv_size(&number);
            } else {
                number = get_unused_port();
                {
                    std::unique_lock<std::mutex> lck(io_map_mtx);
                    pado_io_map[number].reserve(num + 1);
                    for (size_t i = 0; i < num; i++) {
                        pado_io_map[number].push_back(nullptr);
                    }
                    created_timestamp_map[number] = timestamp;
                }
    
                io->send_size(&number);
                io->flush();
            }
            // printf("end listen pado io %d %d\n", number, has_number);
            {
                std::unique_lock<std::mutex> lck(io_map_mtx);
                pado_io_map[number][has_number] = io.release();
    
                auto is_some_null = [](const vector<PadoIO*>& vec) {
                    bool res = false;
                    for (size_t i = 0; i < vec.size(); i++) {
                        if (vec[i] == nullptr) {
                            res = true;
                        }
                    }
                    return res;
                };
    
                if (!is_some_null(pado_io_map[number])) {
                    pado_io_map[number].push_back(nullptr);
                    io_map_cond.notify_one();
                }
            }
        } catch (std::exception& e) {
            printf("error exchange number:%s\n", e.what());
        }
    }

    static void init_pado_io2(IOManager* iomanager, unique_ptr<PadoIO> io) {
        iomanager->init_pado_io(std::move(io));
    }

    void listen(int port) {
        while (1) {
            unique_ptr<PadoIO> io;
            try {
                if (!listen_flag) {
                    clear_pado_io(true);
                    break;
                }
                io.reset(NewPadoIO(port));
                auto t = std::thread(init_pado_io2, this, std::move(io));
                t.detach();
    
                clear_pado_io(false);
            } catch (std::exception& e) {
                printf("listen pado io error:%s\n", e.what());
                bool too_much_exception = check_exception_frequency();
                if (too_much_exception)
                    abort();
                continue;
            } catch (...) {
                printf("listen pado io error:unknown\n");
                bool too_much_exception = check_exception_frequency();
                if (too_much_exception)
                    abort();
                continue;
            }
        }
    }

    IOPtr<PadoIO> accept(size_t num) {
        while (1) {
            std::unique_lock<std::mutex> lck(io_map_mtx);
            for (auto iter = pado_io_map.begin(); iter != pado_io_map.end(); iter++) {
                if (iter->second.size() > num) { // the last will be nullptr
                    IOPtr<PadoIO> pado_ios(num);
                    for (size_t i = 0; i < num; i++) {
                        pado_ios << iter->second[i];
                    }
                    uint32_t port = iter->first;
                    pado_io_map.erase(iter);
    
                    auto timestamp_iter = created_timestamp_map.find(port);
                    if (timestamp_iter != created_timestamp_map.end()) {
                        created_timestamp_map.erase(timestamp_iter);
                    }
    
                    free_used_port(port);
                    return pado_ios;
                }
            }
            io_map_cond.wait(lck);
        }
        return IOPtr<PadoIO>();
    }


};

PortManager* PortManager::instance() {
    static PortManager portManager;
    return &portManager;
}

void create_pado_io(int port) {
    shared_ptr<IOManager> iomanager(new IOManager());
    PortManager::instance()->put(port, iomanager);
}

void listen_pado_io(int port) {
    shared_ptr<IOManager> iomanager = PortManager::instance()->get(port);
    iomanager->listen(port);
}

void stop_listen(int port) {
    shared_ptr<IOManager> iomanager = PortManager::instance()->get(port);
    iomanager->stop_listen();
}

IOPtr<PadoIO> accept_pado_io(int port, size_t num) {
    shared_ptr<IOManager> iomanager = PortManager::instance()->get(port);
    return iomanager->accept(num);
}

void do_connect_pado_io(PadoIO* io, uint32_t* number, uint8_t has_number, string* connect_errormsg, std::mutex* connect_mtx, std::condition_variable* connect_cond) {
    std::function<void()> tryFn = [io, number, has_number, connect_errormsg, connect_mtx, connect_cond]() {
        uint8_t has_number2 = has_number;
        io->Init();
        io->send_size(&has_number2);
        io->flush();
        if (has_number2) {
            string errmsg;
            {
                unique_lock<std::mutex> lck(*connect_mtx);
                while (connect_errormsg->empty() && *number == 0) {
                    connect_cond->wait(lck);
                }
                errmsg = *connect_errormsg;
            }
            if (errmsg.empty()) {
                io->send_size(number);
                io->flush();
            }
        }
        else {
            uint32_t recv_number;
            io->recv_size(&recv_number);
            {
                unique_lock<std::mutex> lck(*connect_mtx);
                *number = recv_number;
                connect_cond->notify_all();
            }
        }
    };

    std::function<void(const char* e)> catchFn = [connect_errormsg, connect_mtx, connect_cond](const char* e) {
        string errormsg = "[connect]error exchange number: " + string(e);
        std::unique_lock<std::mutex> lck(*connect_mtx);
        *connect_errormsg = errormsg;
        connect_cond->notify_all();
    };

    FunctionWrapperV3(tryFn, catchFn)();
}

IOPtr<PadoIO> connect_pado_io(size_t num, const char* url) {
    enable_mpc(0);
    IOPtr<PadoIO> pado_ios(num);
    printf("connect pado io %s\n", url);

    vector<std::thread*> connect_threads;
    string connect_errormsg;
    std::mutex connect_mtx;
    std::condition_variable connect_cond;
    uint32_t number = 0;
    for (size_t i = 0; i < num; i++) {
        // printf("begin create connect pado io %d %d\n", number, i);
        pado_ios << NewPadoIO(url);

        uint8_t has_number = i;
        PadoIO* io = pado_ios[i];
        std::thread* t = new thread(do_connect_pado_io, io, &number, has_number, &connect_errormsg, &connect_mtx, &connect_cond);
        connect_threads.push_back(t);
        // printf("end create connect pado io %d %d\n", number, i);
    }

    for (size_t i = 0; i < connect_threads.size(); i++) {
        connect_threads[i]->join();
        delete connect_threads[i];
    }
    if (!connect_errormsg.empty()) {
        throw PadoNetworkError(connect_errormsg);
    }
    printf("finish connect pado io %s\n", url);
    return pado_ios;
}
IOPtr<PadoIO> new_pado_io(int pado, const char* url, int port) {
    int num = get_pado_io_num();
    IOPtr<PadoIO> pado_ios;
    try {
        if (pado) {
            pado_ios = accept_pado_io(port, num);
        } else {
            pado_ios = connect_pado_io(num, url);
        }
    } catch (std::exception& e) {
        throw PadoNetworkError(string("new pado io error: ") + e.what());
    }
    return pado_ios;
}
