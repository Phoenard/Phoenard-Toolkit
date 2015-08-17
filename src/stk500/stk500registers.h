#ifndef STK500REGISTERS_H
#define STK500REGISTERS_H

#include "stk500.h"
#include <QDebug>

#define CHIPREG_COUNT 300
#define CHIPREG_DATA_COLUMNS 13

typedef struct ChipRegisterInfo {
    ChipRegisterInfo();
    ChipRegisterInfo(QStringList &values);
    void load(QStringList &values);

    QString values[CHIPREG_DATA_COLUMNS];
    int index;
    int addressValue;
    QString address;
    QString name;
    QString module;
    QString function;
    QString bitNames[8];
} ChipRegisterInfo;

class ChipRegisters {
public:
    ChipRegisters();
    int count() const { return sizeof(regData); }
    void resetUserChanges();
    bool changed(int address);
    bool hasUserChanges();
    quint8* data() { return regData; }
    quint8 error(int address);
    const ChipRegisterInfo &info(int address);
    const ChipRegisterInfo &infoHeader() { return info(-1); }
    const ChipRegisterInfo &infoByIndex(int index);

    quint8 operator [](int i) const { return regData[i]; }
    quint8& operator [](int i) { return regData[i]; }

    friend class stk500registers;

protected:
    quint8 regData[CHIPREG_COUNT];      // Live values, changed by the user
    quint8 regDataRead[CHIPREG_COUNT];  // Live values, original to track user changes
    quint8 regDataLast[CHIPREG_COUNT];  // Live values from last time
    quint8 regDataError[CHIPREG_COUNT]; // Contains the bits it failed to write out last time
private:
    static ChipRegisterInfo registerInfo[CHIPREG_COUNT];
    static ChipRegisterInfo registerInfoHeader;
    static ChipRegisterInfo* registerInfoByIndex[CHIPREG_COUNT];
    static bool registerInfoInit;
};

class stk500registers
{
public:
    stk500registers(stk500 *handler) : _handler(handler) {}
    void write(ChipRegisters &registers);
    void read(ChipRegisters &registers);

private:
    stk500 *_handler;
};

#endif // STK500REGISTERS_H
