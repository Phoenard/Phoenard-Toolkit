#ifndef STK500REGISTERS_H
#define STK500REGISTERS_H

#include "stk500.h"

class ChipRegisters {
public:
    ChipRegisters();
    quint8& operator[] (const int nIndex);
    int count() const { return sizeof(regData); }
    void resetChanges();
    bool changed(int index);
    quint8* data() { return regData; }

private:
    quint8 regData[310];
    quint8 regDataLast[310];
};

class stk500registers
{
public:
    stk500registers(stk500 *handler) : _handler(handler) {}
    void write(const ChipRegisters &registers);
    void read(ChipRegisters &registers);

private:
    stk500 *_handler;
    ChipRegisters lastReg;
};

#endif // STK500REGISTERS_H
