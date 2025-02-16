#pragma once

#include <exception>
#include <string>
#include <memory>
#include "macro.h"
using namespace std;

class BaseException : public std::exception {
   public:
    BaseException() {}
    virtual ~BaseException() {}
    const char* what() const noexcept override {
        return errorMsg.c_str();
    }

   public:
    virtual int GetErrorCode() = 0;
    virtual string GetErrorType() = 0;
   protected:
    string errorMsg;
};

class BaseExceptionFactory {
    public:
    virtual shared_ptr<BaseException> createException(const string& e) = 0;
    virtual int GetErrorCode() = 0;
    virtual string GetErrorType() = 0;
};

struct ExceptionFactoryItem {
    ExceptionFactoryItem* next;
    BaseExceptionFactory* factory;
    ExceptionFactoryItem(BaseExceptionFactory* f) {
        factory = f;
        next = nullptr;
    }
};

void pushExceptionFactory(ExceptionFactoryItem* factoryItem);
BaseExceptionFactory* getExceptionFactory(const string& e);
string getExceptionMsg(const string& e);
void printAllException();

#define MAKE_ALL_EXCEPTIONS()                                 \
    MAKE_EXCEPTION(10001, SocketError);                       \
    MAKE_EXCEPTION(10002, NetworkError);                      \
    MAKE_EXCEPTION(10003, PrimusServerNetworkError);          \
    MAKE_EXCEPTION(10004, TlsNetworkError);                   \
                                                              \
    MAKE_EXCEPTION(20001, LengthException);                   \
    MAKE_EXCEPTION(20002, OutOfRangeException);               \
    MAKE_EXCEPTION(20003, InvalidArgumentException);          \
    MAKE_EXCEPTION(20004, LogicError);                        \
    MAKE_EXCEPTION(20005, RuntimeError);                      \
                                                              \
    MAKE_EXCEPTION(30001, ResponseException);                 \
    MAKE_EXCEPTION(30002, ResponseCheckError);                \
    MAKE_EXCEPTION(30003, ParseError);                        \
    MAKE_EXCEPTION(30004, ParseJsonError);                    \
    MAKE_EXCEPTION(30005, ParseHtmlError);                    \
                                                              \
    MAKE_EXCEPTION(40001, FileNotExistException);             \
    MAKE_EXCEPTION(40002, SSLCertificateError);               \
                                                              \
    MAKE_EXCEPTION(50001, InternalError);                     \
    MAKE_EXCEPTION(50002, MPCThreadException);                \
    MAKE_EXCEPTION(50003, PlainThreadException);              \
    MAKE_EXCEPTION(50004, PlainClientNotStarted);             \
    MAKE_EXCEPTION(50005, WaitPlainClientTimeout);            \
    MAKE_EXCEPTION(50006, MPCTClientNotStarted);              \
    MAKE_EXCEPTION(50007, DecryptException);                  \
    MAKE_EXCEPTION(50008, IzkCheckException);                 \
    MAKE_EXCEPTION(50009, RunTooSlow);                        \
    MAKE_EXCEPTION(50010, EvaluatorCheatedError);             \
    MAKE_EXCEPTION(50011, ExtendedMasterSecretError);         \
                                                              \
    MAKE_EXCEPTION(99999, OtherError);                        \


#ifndef MAKE_EXCEPTION
#define MAKE_EXCEPTION(code, cls)                                                    \
    class cls : public BaseException {                                               \
       public:                                                                       \
        cls(const string& e) : BaseException() {                                     \
            _printf("%d|%s|%s\n", GetErrorCode(), GetErrorType().c_str(), e.c_str()); \
            fflush(stdout);                                                          \
            errorMsg = "[" + GetErrorType() + "]" + e;                               \
        }                                                                            \
                                                                                     \
       public:                                                                       \
        int GetErrorCode() { return code; }                                          \
        string GetErrorType() { return string(#cls); }                               \
    };                                                                               \
                                                                                     \
    class cls##Factory : public BaseExceptionFactory {                               \
        public:                                                                      \
        cls##Factory() = default;                                                    \
        shared_ptr<BaseException> createException(const string& e) override {        \
            return shared_ptr<BaseException>(new cls(e));                            \
        }                                                                            \
        int GetErrorCode() override { return code; }                                 \
        string GetErrorType() override {                                             \
            return string(#cls);                                                     \
        }                                                                            \
    };                                                                               \
                                                                                     \
    class cls##FactoryCreator {                                                      \
        public:                                                                      \
        cls##FactoryCreator() {                                                      \
            static cls##Factory factory;                                             \
            static ExceptionFactoryItem item(&factory);                              \
            pushExceptionFactory(&item);                                             \
        }                                                                            \
    };                                                                               \
                                                                                     

MAKE_ALL_EXCEPTIONS();
#undef MAKE_EXCEPTION
#endif

