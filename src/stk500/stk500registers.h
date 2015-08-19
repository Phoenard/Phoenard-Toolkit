#ifndef STK500REGISTERS_H
#define STK500REGISTERS_H

#include "stk500.h"
#include <QDebug>

#define CHIPREG_ADDR_START    0x20
#define CHIPREG_ADDR_END      0x1FF
#define CHIPREG_BUFFSIZE      (CHIPREG_ADDR_END+1)
#define CHIPREG_COUNT         (CHIPREG_BUFFSIZE-CHIPREG_ADDR_START)
#define CHIPREG_DATA_COLUMNS  13
#define PINMAP_DATA_COLUMNS   5

typedef struct ChipRegisterInfo {
    ChipRegisterInfo();
    ChipRegisterInfo(const QStringList &values);
    void load(const QStringList &values);

    QString values[CHIPREG_DATA_COLUMNS];
    int index;
    int addressValue;
    QString address;
    QString name;
    QString module;
    QString function;
    QString bitNames[8];
} ChipRegisterInfo;

typedef struct PinMapInfo {
    PinMapInfo();
    PinMapInfo(const QStringList &values);
    void load(const QStringList &values);

    QString values[PINMAP_DATA_COLUMNS];
    int index;
    int pin;
    int addr_pin;
    int addr_ddr;
    int addr_port;
    int addr_mask;
    QString port;
    QString name;
    QString module;
    QString function;
} PinMapInfo;

class ChipRegisters {
public:
    ChipRegisters();
    int count() const { return CHIPREG_COUNT; }
    void resetUserChanges();
    bool changed(int address);
    bool hasUserChanges();
    quint8* data() { return regData; }
    quint8 error(int address);
    const ChipRegisterInfo &info(int address);
    const ChipRegisterInfo &infoHeader();
    const ChipRegisterInfo &infoByIndex(int index);
    static int findRegisterAddress(const QString &name);
    const QList<PinMapInfo> &pinmap();
    const PinMapInfo &pinmapHeader();

    quint8 operator [](int i) const { return regData[i]; }
    quint8& operator [](int i) { return regData[i]; }

    friend class stk500registers;

protected:
    quint8 regData[CHIPREG_BUFFSIZE];      // Live values, changed by the user
    quint8 regDataRead[CHIPREG_BUFFSIZE];  // Live values, original to track user changes
    quint8 regDataLast[CHIPREG_BUFFSIZE];  // Live values from last time
    quint8 regDataError[CHIPREG_BUFFSIZE]; // Contains the bits it failed to write out last time
private:
    static void initRegisters();
    static ChipRegisterInfo registerInfo[CHIPREG_BUFFSIZE];
    static ChipRegisterInfo registerInfoHeader;
    static ChipRegisterInfo* registerInfoByIndex[CHIPREG_COUNT];
    static QList<PinMapInfo> pinmapInfo;
    static PinMapInfo pinmapInfoHeader;
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
