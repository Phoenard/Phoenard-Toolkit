#ifndef MAINMENUTAB_H
#define MAINMENUTAB_H

#include "stk500/stk500serial.h"

class MainMenuTab
{
public:
    virtual void setSerial(stk500Serial *serial) { this->serial = serial; }

protected:
    stk500Serial *serial;
};

#endif // MAINMENUTAB_H
