#ifndef STK500SERVICE_H
#define STK500SERVICE_H

#include "stk500.h"
#include <QDebug>

#define BOOT_START_ADDR 0x3E000

class stk500service
{
public:
    stk500service(stk500 *handler) : _handler(handler) {}
    void begin();
    void end();
    void readPage(quint32 address, char* pageData);
    void writePage(quint32 address, const char* pageData);

private:
    void checkConnection();
    int readSerial(char* output, int limit);
    bool writeVerifyRetry(const char* data, int dataLen);
    bool writeVerify(const char* data, int dataLen);
    void setAddress(quint32 address);
    void flushData();
    stk500 *_handler;
};

#endif // STK500SERVICE_H
