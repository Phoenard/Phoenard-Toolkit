#ifndef STK500REGISTERS_H
#define STK500REGISTERS_H

#include "stk500.h"

// A few constants for the chip register information
#define CHIPREG_OFFSET 0x20
#define CHIPREG_COUNT 278

// Makes use of a macro to use these values both as enum and as String
#define CHIPREG_V(index, name) REG_##name = (index - CHIPREG_OFFSET) ,
#define CHIPREG_ENUM_VALUES \
    CHIPREG_V(0x136, UDR3) \
    CHIPREG_V(0x135, UBRR3H) \
    CHIPREG_V(0x134, UBRR3L) \
    CHIPREG_V(0x132, UCSR3C) \
    CHIPREG_V(0x131, UCSR3B) \
    CHIPREG_V(0x130, UCSR3A) \
    CHIPREG_V(0x12D, OCR5CH) \
    CHIPREG_V(0x12C, OCR5CL) \
    CHIPREG_V(0x12B, OCR5BH) \
    CHIPREG_V(0x12A, OCR5BL) \
    CHIPREG_V(0x129, OCR5AH) \
    CHIPREG_V(0x128, OCR5AL) \
    CHIPREG_V(0x127, ICR5H) \
    CHIPREG_V(0x126, ICR5L) \
    CHIPREG_V(0x125, TCNT5H) \
    CHIPREG_V(0x124, TCNT5L) \
    CHIPREG_V(0x122, TCCR5C) \
    CHIPREG_V(0x121, TCCR5B) \
    CHIPREG_V(0x120, TCCR5A) \
    CHIPREG_V(0x10B, PORTL) \
    CHIPREG_V(0x10A, DDRL) \
    CHIPREG_V(0x109, PINL) \
    CHIPREG_V(0x108, PORTK) \
    CHIPREG_V(0x107, DDRK) \
    CHIPREG_V(0x106, PINK) \
    CHIPREG_V(0x105, PORTJ) \
    CHIPREG_V(0x104, DDRJ) \
    CHIPREG_V(0x103, PINJ) \
    CHIPREG_V(0x102, PORTH) \
    CHIPREG_V(0x101, DDRH) \
    CHIPREG_V(0x100, PINH) \
    CHIPREG_V(0xD6, UDR2) \
    CHIPREG_V(0xD5, UBRR2H) \
    CHIPREG_V(0xD4, UBRR2L) \
    CHIPREG_V(0xD2, UCSR2C) \
    CHIPREG_V(0xD1, UCSR2B) \
    CHIPREG_V(0xD0, UCSR2A) \
    CHIPREG_V(0xCE, UDR1) \
    CHIPREG_V(0xCD, UBRR1H) \
    CHIPREG_V(0xCC, UBRR1L) \
    CHIPREG_V(0xCA, UCSR1C) \
    CHIPREG_V(0xC9, UCSR1B) \
    CHIPREG_V(0xC8, UCSR1A) \
    CHIPREG_V(0xC6, UDR0) \
    CHIPREG_V(0xC5, UBRR0H) \
    CHIPREG_V(0xC4, UBRR0L) \
    CHIPREG_V(0xC2, UCSR0C) \
    CHIPREG_V(0xC1, UCSR0B) \
    CHIPREG_V(0xC0, UCSR0A) \
    CHIPREG_V(0xBD, TWAMR) \
    CHIPREG_V(0xBC, TWCR) \
    CHIPREG_V(0xBB, TWDR) \
    CHIPREG_V(0xBA, TWAR) \
    CHIPREG_V(0xB9, TWSR) \
    CHIPREG_V(0xB8, TWBR) \
    CHIPREG_V(0xB6, ASSR) \
    CHIPREG_V(0xB4, OCR2B) \
    CHIPREG_V(0xB3, OCR2A) \
    CHIPREG_V(0xB2, TCNT2) \
    CHIPREG_V(0xB1, TCCR2B) \
    CHIPREG_V(0xB0, TCCR2A) \
    CHIPREG_V(0xAD, OCR4CH) \
    CHIPREG_V(0xAC, OCR4CL) \
    CHIPREG_V(0xAB, OCR4BH) \
    CHIPREG_V(0xAA, OCR4BL) \
    CHIPREG_V(0xA9, OCR4AH) \
    CHIPREG_V(0xA8, OCR4AL) \
    CHIPREG_V(0xA7, ICR4H) \
    CHIPREG_V(0xA6, ICR4L) \
    CHIPREG_V(0xA5, TCNT4H) \
    CHIPREG_V(0xA4, TCNT4L) \
    CHIPREG_V(0xA2, TCCR4C) \
    CHIPREG_V(0xA1, TCCR4B) \
    CHIPREG_V(0xA0, TCCR4A) \
    CHIPREG_V(0x9D, OCR3CH) \
    CHIPREG_V(0x9C, OCR3CL) \
    CHIPREG_V(0x9B, OCR3BH) \
    CHIPREG_V(0x9A, OCR3BL) \
    CHIPREG_V(0x99, OCR3AH) \
    CHIPREG_V(0x98, OCR3AL) \
    CHIPREG_V(0x97, ICR3H) \
    CHIPREG_V(0x96, ICR3L) \
    CHIPREG_V(0x95, TCNT3H) \
    CHIPREG_V(0x94, TCNT3L) \
    CHIPREG_V(0x92, TCCR3C) \
    CHIPREG_V(0x91, TCCR3B) \
    CHIPREG_V(0x90, TCCR3A) \
    CHIPREG_V(0x8D, OCR1CH) \
    CHIPREG_V(0x8C, OCR1CL) \
    CHIPREG_V(0x8B, OCR1BH) \
    CHIPREG_V(0x8A, OCR1BL) \
    CHIPREG_V(0x89, OCR1AH) \
    CHIPREG_V(0x88, OCR1AL) \
    CHIPREG_V(0x87, ICR1H) \
    CHIPREG_V(0x86, ICR1L) \
    CHIPREG_V(0x85, TCNT1H) \
    CHIPREG_V(0x84, TCNT1L) \
    CHIPREG_V(0x82, TCCR1C) \
    CHIPREG_V(0x81, TCCR1B) \
    CHIPREG_V(0x80, TCCR1A) \
    CHIPREG_V(0x7F, DIDR1) \
    CHIPREG_V(0x7E, DIDR0) \
    CHIPREG_V(0x7D, DIDR2) \
    CHIPREG_V(0x7C, ADMUX) \
    CHIPREG_V(0x7B, ADCSRB) \
    CHIPREG_V(0x7A, ADCSRA) \
    CHIPREG_V(0x79, ADCH) \
    CHIPREG_V(0x78, ADCL) \
    CHIPREG_V(0x75, XMCRB) \
    CHIPREG_V(0x74, XMCRA) \
    CHIPREG_V(0x73, TIMSK5) \
    CHIPREG_V(0x72, TIMSK4) \
    CHIPREG_V(0x71, TIMSK3) \
    CHIPREG_V(0x70, TIMSK2) \
    CHIPREG_V(0x6F, TIMSK1) \
    CHIPREG_V(0x6E, TIMSK0) \
    CHIPREG_V(0x6D, PCMSK2) \
    CHIPREG_V(0x6C, PCMSK1) \
    CHIPREG_V(0x6B, PCMSK0) \
    CHIPREG_V(0x6A, EICRB) \
    CHIPREG_V(0x69, EICRA) \
    CHIPREG_V(0x68, PCICR) \
    CHIPREG_V(0x66, OSCCAL) \
    CHIPREG_V(0x65, PRR1) \
    CHIPREG_V(0x64, PRR0) \
    CHIPREG_V(0x61, CLKPR) \
    CHIPREG_V(0x60, WDTCSR) \
    CHIPREG_V(0x5F, SREG) \
    CHIPREG_V(0x5E, SPH) \
    CHIPREG_V(0x5D, SPL) \
    CHIPREG_V(0x5C, EIND) \
    CHIPREG_V(0x5B, RAMPZ) \
    CHIPREG_V(0x57, SPMCSR) \
    CHIPREG_V(0x55, MCUCR) \
    CHIPREG_V(0x54, MCUSR) \
    CHIPREG_V(0x53, SMCR) \
    CHIPREG_V(0x51, OCDR) \
    CHIPREG_V(0x50, ACSR) \
    CHIPREG_V(0x4E, SPDR) \
    CHIPREG_V(0x4D, SPSR) \
    CHIPREG_V(0x4C, SPCR) \
    CHIPREG_V(0x4B, GPIOR2) \
    CHIPREG_V(0x4A, GPIOR1) \
    CHIPREG_V(0x48, OCR0B) \
    CHIPREG_V(0x47, OCR0A) \
    CHIPREG_V(0x46, TCNT0) \
    CHIPREG_V(0x45, TCCR0B) \
    CHIPREG_V(0x44, TCCR0A) \
    CHIPREG_V(0x43, GTCCR) \
    CHIPREG_V(0x42, EEARH) \
    CHIPREG_V(0x41, EEARL) \
    CHIPREG_V(0x40, EEDR) \
    CHIPREG_V(0x3F, EECR) \
    CHIPREG_V(0x3E, GPIOR0) \
    CHIPREG_V(0x3D, EIMSK) \
    CHIPREG_V(0x3C, EIFR) \
    CHIPREG_V(0x3B, PCIFR) \
    CHIPREG_V(0x3A, TIFR5) \
    CHIPREG_V(0x39, TIFR4) \
    CHIPREG_V(0x38, TIFR3) \
    CHIPREG_V(0x37, TIFR2) \
    CHIPREG_V(0x36, TIFR1) \
    CHIPREG_V(0x35, TIFR0) \
    CHIPREG_V(0x34, PORTG) \
    CHIPREG_V(0x33, DDRG) \
    CHIPREG_V(0x32, PING) \
    CHIPREG_V(0x31, PORTF) \
    CHIPREG_V(0x30, DDRF) \
    CHIPREG_V(0x2F, PINF) \
    CHIPREG_V(0x2E, PORTE) \
    CHIPREG_V(0x2D, DDRE) \
    CHIPREG_V(0x2C, PINE) \
    CHIPREG_V(0x2B, PORTD) \
    CHIPREG_V(0x2A, DDRD) \
    CHIPREG_V(0x29, PIND) \
    CHIPREG_V(0x28, PORTC) \
    CHIPREG_V(0x27, DDRC) \
    CHIPREG_V(0x26, PINC) \
    CHIPREG_V(0x25, PORTB) \
    CHIPREG_V(0x24, DDRB) \
    CHIPREG_V(0x23, PINB) \
    CHIPREG_V(0x22, PORTA) \
    CHIPREG_V(0x21, DDRA) \
    CHIPREG_V(0x20, PINA)

enum ChipReg : int {
    CHIPREG_ENUM_VALUES
};

class ChipRegisters {
public:
    ChipRegisters();
    int count() const { return sizeof(regData); }
    void resetChanges();
    bool changed(int index);
    quint8* data() { return regData; }
    QString name(int index);
    int addr(int index) const { return index + CHIPREG_OFFSET; }

    quint8 operator [](int i) const { return regData[i]; }
    quint8& operator [](int i) { return regData[i]; }
    quint8 operator [](ChipReg reg) const { return regData[(int) reg]; }
    quint8& operator [](ChipReg reg) { return regData[(int) reg]; }

private:
    quint8 regData[CHIPREG_COUNT];
    quint8 regDataLast[CHIPREG_COUNT];
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
