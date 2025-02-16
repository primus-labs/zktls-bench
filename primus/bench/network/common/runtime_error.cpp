#include "runtime_error.h"

#ifndef MAKE_EXCEPTION
#define MAKE_EXCEPTION(code, cls)                                   \
    static cls##FactoryCreator cls##FactoryCreatorInstance;

MAKE_ALL_EXCEPTIONS();
#undef MAKE_EXCEPTION
#endif

static ExceptionFactoryItem* exceptionFactoryHead = nullptr;

void pushExceptionFactory(ExceptionFactoryItem* item) {
    if (exceptionFactoryHead == nullptr) {
        exceptionFactoryHead = item;
    }
    else {
        ExceptionFactoryItem* h = exceptionFactoryHead;
        while (h->next != nullptr) {
            h = h->next;
        }
        h->next = item;
    }
}

void printAllException() {
    ExceptionFactoryItem* h = exceptionFactoryHead;
    _printf("print all exception:\n");
    while (h != nullptr) {
        _printf("    %d -> %s\n", h->factory->GetErrorCode(), h->factory->GetErrorType().c_str());
        h = h->next;
    }
}

BaseExceptionFactory* getExceptionFactory(const string& e) {
    if (e[0] != '[') {
        return nullptr;
    }
    size_t pos = e.find("]", 1);
    if (pos == string::npos) {
        return nullptr;
    }

    string exceptionClass = e.substr(1, pos - 1);
    string exceptionMsg = e.substr(pos + 1, e.size() - (pos + 1));

    ExceptionFactoryItem* h = exceptionFactoryHead;
    while (h != nullptr) {
        if (h->factory->GetErrorType() == exceptionClass) {
            return h->factory;
        }
        h = h->next;
    }
    return nullptr;
}

string getExceptionMsg(const string& e) {
    if (e[0] != '[') {
        return "";
    }
    size_t pos = e.find("]", 1);
    if (pos == string::npos) {
        return "";
    }

    string exceptionClass = e.substr(1, pos - 1);
    string exceptionMsg = e.substr(pos + 1, e.size() - (pos + 1));
    return exceptionMsg;
}
