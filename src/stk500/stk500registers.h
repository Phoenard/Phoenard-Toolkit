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
#define ANALOG_PIN_COUNT      16
#define ANALOG_PIN_INCREMENT  4

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
    void resetChanges();
    void applyUserChanges(const ChipRegisters& other);
    bool changed(int address);
    bool hasUserChanges();
    bool getChangedRange(int* address, int* count);
    quint8* data(int addrStart = CHIPREG_ADDR_START) { return regData + addrStart; }
    quint8 error(int address);
    quint16 analog(int pinNr) const { return analogData[pinNr]; }
    const ChipRegisterInfo &info(int address);
    const ChipRegisterInfo &infoHeader();
    const ChipRegisterInfo &infoByIndex(int index);
    const QList<PinMapInfo> &pinmap();
    const PinMapInfo &pinmapHeader();

    // Initialize the registers for a Hardware Serial UART
    void setupUART(int idx, qint32 baudRate);

    // Getters / Setters with change tracking
    quint8 get(int address) const;
    quint8 get(const QString &name) const;
    void set(int address, quint8 value);
    void set(const QString &name, quint8 value);
    void setBit(int address, quint8 mask, bool bitSet);
    quint8 setXor(int address, quint8 mask);
    bool getPin(const QString& module, const QString& function);
    bool getPin(int pin);
    void setPin(const QString& module, const QString& function, bool mode, bool state);
    void setPin(int pin, bool mode, bool state);

    // Only getters are allowed for array access operator
    quint8 operator [](int address) const { return get(address); }
    quint8 operator [](const QString &name) const { return get(name); }

    // Stack accessors to register and pinmap tables
    static int findRegisterAddress(const QString &name);
    static PinMapInfo &findPinInfo(const QString module, const QString &function);
    static PinMapInfo &findPinInfo(int pin);

    friend class stk500registers;

protected:
    quint8 regData[CHIPREG_BUFFSIZE];      // Live values, changed by the user
    quint8 regDataRead[CHIPREG_BUFFSIZE];  // Live values, original to track user changes
    quint8 regDataLast[CHIPREG_BUFFSIZE];  // Live values from last time
    quint8 regDataError[CHIPREG_BUFFSIZE]; // Contains the bits it failed to write out last time
    quint16 analogData[ANALOG_PIN_COUNT];  // Live updates analog input values
    quint8 analogDataIndex;                // Index of the analog pin to be refreshed next
    bool regDataWasRead;                   // Whether the register data was previously read
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
    void readADC(ChipRegisters &registers);

private:
    stk500 *_handler;
};

#endif // STK500REGISTERS_H
