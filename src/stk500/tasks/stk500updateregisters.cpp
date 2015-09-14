#include "../stk500task.h"

void stk500UpdateRegisters::run() {
    protocol->reg().write(this->reg);
    if (read) {
        protocol->reg().read(this->reg);
    }
    if (readADC) {
        protocol->reg().readADC(this->reg);
    }
}
