#pragma once
#include <memory>
#include <unistd.h>

template<class T>
struct FreeFn {
    typedef void (*deleter)(T*);
};

template<>
struct FreeFn<FILE> {
    typedef int (*deleter)(FILE*);
};

struct TCPSocket {
    private:
    int fd = -1;
    bool close_on_delete = true;

    public:
    TCPSocket(int fd, bool close_on_delete) {
        this->fd = fd;
        this->close_on_delete = close_on_delete;
    }
    ~TCPSocket() {
        if (this->close_on_delete) {
            close(this->fd);
        }
    }
    int GetFd() const {
        return this->fd;
    }
    void SetCloseOnDelete(bool b) {
        this->close_on_delete = b;
    }
};


template<class T, class D = typename FreeFn<T>::deleter>
struct StdUniquePtr {
    typedef std::unique_ptr<T, D> Type;
};

template<class T, class S>
struct SkDeleter {
public:
    typedef int (*NumFn)(const T*);
    typedef S* (*ValueFn)(const T*, int);
    typedef void (*FreeT)(T*);
    typedef void (*FreeS)(S*);
    SkDeleter(NumFn numFn, ValueFn valueFn, FreeT freeT, FreeS freeS) {
        this->numFn = numFn;
        this->valueFn = valueFn;
        this->freeT = freeT;
        this->freeS = freeS;
    }

    void operator()(T* t) {
        int num = numFn(t);
        for (int i = 0; i < num; i++) {
            S* s = valueFn(t, i);
            freeS(s);
        }
        freeT(t);
    }
private:
    NumFn numFn;
    ValueFn valueFn;
    FreeT freeT;
    FreeS freeS;
};

#define SK_DELETER_TYPE(S) \
    SkDeleter<STACK_OF(S), S>

#define SK_DELETER(S) \
    SkDeleter<STACK_OF(S), S>(sk_##S##_num, sk_##S##_value, sk_##S##_free, S##_free)

#define STACK_PTR_TYPE_OF(S) \
    StdUniquePtr<STACK_OF(S), SK_DELETER_TYPE(S)>::Type

template<class IO>
class IOPtr {
public:
    explicit IOPtr(int num = 0) {
        if (num > 0) {
            reserve(num);
        }
    }
    IOPtr(IOPtr&& rhs) {
        this->ios = rhs.ios;
        this->num = rhs.num;
        this->index = rhs.index;
        rhs.ios = nullptr;
        rhs.num = 0;
        rhs.index = 0;
    }

    IOPtr& operator=(IOPtr&& rhs) {
        release();
        this->ios = rhs.ios;
        this->num = rhs.num;
        this->index = rhs.index;
        rhs.ios = nullptr;
        rhs.num = 0;
        rhs.index = 0;
        return *this;
    }
    ~IOPtr() {
        release();
    }

    IO* operator[](size_t i) {
        return ios[i];
    }
    operator IO** () {
        return ios;
    }
    size_t size() {
        return num;
    }
    void resize(int num) {
        release();
        reserve(num);
    }

    void operator << (IO* io) {
        ios[index++] = io;
    }

private:
    void reserve(int num) {
        ios = new IO*[num];
        this->num = num;
    }
    void release() {
        if (index > 0) {
            for (int i = 0; i < index; i++) {
                delete ios[i];
            }
            delete []ios;
        }
    }

private:
    IO** ios = nullptr;
    int num = 0;
    int index = 0;
};
    
template<typename T>
struct ThreadVariableWrapper {
    private:
    T obj;
    T** pp;
    public:
    ThreadVariableWrapper(T** pp) {
        this->pp = pp;
        *pp = &obj;
    }
    ~ThreadVariableWrapper() {
        if (*pp != nullptr) {
            *pp = nullptr;
        }
    }
    T* unwrap() {
        return &obj;
    }
    T& operator*() {
        return obj;
    }
};

