#include "stk500registers.h"

ChipRegisters::ChipRegisters() {
    memset(regData, 0, sizeof(regData));
    memset(regDataLast, 0, sizeof(regDataLast));
    memset(regDataRead, 0, sizeof(regDataRead));
}

bool ChipRegisters::changed(int index) {
    return regDataLast[index] != regData[index];
}

void ChipRegisters::resetChanges() {
    memcpy(regDataLast, regData, sizeof(regDataLast));
}

// Redefine the values to represent case statements inside the function
#undef CHIPREG_V
#define CHIPREG_V(index, name) case (index - CHIPREG_OFFSET) : return #name;

QString ChipRegisters::name(int index) {
    switch (index) {
    CHIPREG_ENUM_VALUES
    default: return "RESERVED";
    }
}

static QStringList bitNamesGeneric(QString name) {
    QStringList names;
    for (int i = 0; i < 8; i++) names << (name + QString::number(i));
    return names;
}

QStringList ChipRegisters::bitNames(int index) {
    QString name = this->name(index);
    if (name.startsWith("PORT") || name.startsWith("PIN") || name.startsWith("DDR")) {
        return bitNamesGeneric("P" + name.right(1));
    }
    if (name == "RESERVED") {
        return QStringList() << "" << "" << "" << "" << "" << "" << "" << "";
    }

    QStringList names;
    switch ((ChipReg) index) {

    case REG_TIFR0:  names << "-" << "-" << "-" << "-" << "-" << "OCF0B" << "OCF0A" << "TOV0"; break;
    case REG_TIFR1:  names << "-" << "-" << "ICF1" << "-" << "OCF1C" << "OCF1B" << "OCF1A" << "TOV1"; break;
    case REG_TIFR2:  names << "-" << "-" << "-" << "-" << "-" << "OCF2B" << "OCF2A" << "TOV2"; break;
    case REG_TIFR3:  names << "-" << "-" << "ICF3" << "-" << "OCF3C" << "OCF3B" << "OCF3A" << "TOV3"; break;
    case REG_TIFR4:  names << "-" << "-" << "ICF4" << "-" << "OCF4C" << "OCF4B" << "OCF4A" << "TOV4"; break;
    case REG_TIFR5:  names << "-" << "-" << "ICF5" << "-" << "OCF5C" << "OCF5B" << "OCF5A" << "TOV5"; break;

    default:
        names << "-" << "-" << "-" << "-" << "-" << "-" << "-" << "-";
        break;
    }
    return names;
}

void stk500registers::write(ChipRegisters &registers) {
    // For any bytes that differ, write the byte to the chip
    // Only write out the bits that changed for added safety
    // There is a possibility to write all bytes out in one go
    // For the moment, we do it safe and 'slow'
    for (int i = 0; i < registers.count(); i++) {
        quint8 changeMask = registers.regData[i] ^ registers.regDataRead[i];
        if (changeMask) {
            registers.regDataRead[i] = _handler->RAM_writeByte(i + CHIPREG_OFFSET, registers[i], changeMask);
        }
    }
}

void stk500registers::read(ChipRegisters &registers) {
    // Clear any changes stored within
    registers.resetChanges();

    // Read the current registers
    _handler->RAM_read(CHIPREG_OFFSET, (char*) registers.regDataRead, registers.count());
    memcpy(registers.regData, registers.regDataRead, sizeof(registers.regData));
}
