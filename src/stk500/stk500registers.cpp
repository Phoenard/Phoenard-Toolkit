#include "stk500registers.h"

bool ChipRegisters::registerInfoInit = false;
ChipRegisterInfo ChipRegisters::registerInfo[CHIPREG_COUNT];
ChipRegisterInfo* ChipRegisters::registerInfoByIndex[CHIPREG_COUNT];
ChipRegisterInfo ChipRegisters::registerInfoHeader;

ChipRegisters::ChipRegisters() {
    memset(regData, 0, sizeof(regData));
    memset(regDataLast, 0, sizeof(regDataLast));
    memset(regDataRead, 0, sizeof(regDataRead));
    memset(regDataError, 0, sizeof(regDataError));
}

bool ChipRegisters::changed(int address) {
    return regDataLast[address] != regData[address];
}

quint8 ChipRegisters::error(int address) {
    return regDataError[address];
}

bool ChipRegisters::hasUserChanges() {
    return memcmp(regDataRead, regData, sizeof(regData)) != 0;
}

void ChipRegisters::resetUserChanges() {
    memcpy(regData, regDataRead, sizeof(regData));
}

ChipRegisterInfo::ChipRegisterInfo() {
    load(QStringList());
}

ChipRegisterInfo::ChipRegisterInfo(QStringList &values) {
    load(values);
}

void ChipRegisterInfo::load(QStringList &values) {
    for (int i = 0; i < CHIPREG_DATA_COLUMNS; i++) {
        this->values[i] = (i < values.count()) ? values[i] : "-";
    }

    this->index = -1;
    this->address = this->values[0];
    this->name = this->values[1];
    this->module = this->values[2];
    this->function = this->values[3];
    for (int i = 0; i < 8; i++) {
        this->bitNames[7-i] = this->values[i + 4];
    }
    bool succ = false;
    this->addressValue = this->address.toInt(&succ, 16);
    if (!succ) this->addressValue = -1;
}

const ChipRegisterInfo &ChipRegisters::info(int address) {
    if (!registerInfoInit) {
        registerInfoInit = true;

        // Default entry if none exists
        ChipRegisterInfo defaultEntry;

        // Read all entries
        QList<ChipRegisterInfo> entries;
        QFile registersFile(":/data/registers.csv");
        if (registersFile.open(QIODevice::ReadOnly)) {
            QTextStream textStream(&registersFile);
            while (!textStream.atEnd()) {
                // Read the next entry
                ChipRegisterInfo entry(textStream.readLine().split(","));

                // Handle different entry types
                if (entry.address == "OTHER") {
                    // OTHER default constant
                    defaultEntry = entry;
                } else if (entry.addressValue == -1) {
                    // Header - no valid address
                    registerInfoHeader = entry;
                } else {
                    // Entry with valid address
                    entries.append(entry);
                }
            }
        }

        // Initialize all register info entries to the default values
        for (int i = 0; i < CHIPREG_COUNT; i++) {
            registerInfo[i] = defaultEntry;
            registerInfo[i].addressValue = i;
            registerInfo[i].address = stk500::getHexText(i);
            registerInfo[i].values[0] = registerInfo[i].address;
        }

        // Store all entries at the addresses
        int index = 0;
        for (ChipRegisterInfo &entry : entries) {
            int addr = entry.addressValue;
            if ((addr >= 0) && (addr < CHIPREG_COUNT)) {
                registerInfo[addr] = entry;
                registerInfo[addr].index = index;
                registerInfoByIndex[index] = &registerInfo[addr];
                index++;
            }
        }

        // Fill the remaining entries with the 'OTHER' registers
        for (int i = 0; i < CHIPREG_COUNT; i++) {
            if (registerInfo[i].index == -1) {
                registerInfo[i].index = index;
                registerInfoByIndex[index] = &registerInfo[i];
                index++;
            }
        }
    }
    if ((address >= 0) && (address < CHIPREG_COUNT)) {
        return registerInfo[address];
    } else {
        return registerInfoHeader;
    }
}

const ChipRegisterInfo &ChipRegisters::infoByIndex(int index) {
    info(-1); // Ensure initialized
    if ((index >= 0) && (index < CHIPREG_COUNT)) {
        return *registerInfoByIndex[index];
    } else {
        return registerInfoHeader;
    }
}

void stk500registers::write(ChipRegisters &registers) {
    // For any bytes that differ, write the byte to the chip
    // Only write out the bits that changed for added safety
    // When done, reset the value to the read one
    for (int i = 0; i < registers.count(); i++) {
        quint8 changeMask = registers.regData[i] ^ registers.regDataRead[i];
        if (changeMask) {
            _handler->RAM_writeByte(i, registers[i], changeMask);
        }
    }
}

void stk500registers::read(ChipRegisters &registers) {
    // Store the previously read values into the last registers
    memcpy(registers.regDataLast, registers.regDataRead, sizeof(registers.regDataLast));

    // Read the current registers
    _handler->RAM_read(0, (char*) registers.regDataRead, registers.count());

    // Compare the last written values to the newly read values
    // If there is a difference, we failed to write those particular bits
    for (int i = 0; i < registers.count(); i++) {
        if (registers.regDataLast[i] != registers.regData[i]) {
            registers.regDataError[i] = registers.regDataRead[i] ^ registers.regData[i];
        } else {
            registers.regDataError[i] = 0;
        }
    }

    // Synchronize the current register values with the last ones read
    memcpy(registers.regData, registers.regDataRead, sizeof(registers.regData));
}
