#include "../stk500task.h"

void stk500UpdateFirmware::init() {
    setUsesFirmware(false);
}

void stk500UpdateFirmware::run() {

    /* Page buffer used while reading / writing */
    char pageData[256];

    /* Verify that the firmware we are programming supports service mode */
    setStatus("Verifying firmware");
    if (!ProgramData::isServicePage(firmwareData.data())) {
        throw ProtocolException("This firmware is incompatible with the firmware that "
                                "is currently installed. Only Phoenard firmware built "
                                "after September 6, 2015, is compatible with service mode programming.\n\n"
                                "You will need to use an ISP programmer to use this firmware. "
                                "If this is old firmware, please download more recent firmware instead.");
    }

    /* Initialize service routine */
    setStatus("Entering service mode");
    protocol->service().begin();

    /* Verify that the service routine flash matches up */
    setStatus("Verifying service routine flash");
    protocol->service().readPage(BOOT_START_ADDR, pageData);
    if (!ProgramData::isServicePage(pageData)) {
        throw ProtocolException("The firmware that is currently installed does not appear "
                                "to contain a valid service routine. If uploading sketches "
                                "is still possible, please contact our support team for help.");
    }

    /*
     * Set second page to jump to service routine for recovery
     * The recovery page solely consists of a watchdog reset
     * and UART initialization, ending in a jump to the service
     * routine. The following code is executed (bootloader):
     *
     * asm volatile ( ".set __stack, %0" :: "i" (RAMEND) );
     * asm volatile ( "clr __zero_reg__" );  // r1 set to 0
     * asm volatile ("cli");
     * asm volatile ("wdr");
     * MCUSR   =  0;
     * WDTCSR |=  _BV(WDCE) | _BV(WDE);
     * WDTCSR  =  0;
     * UART_STATUS_REG    |=  (1 <<UART_DOUBLE_SPEED);
     * UART_BAUD_RATE_LOW  =  UART_BAUD_SELECT;
     * UART_CONTROL_REG    =  (1 << UART_ENABLE_RECEIVER) | (1 << UART_ENABLE_TRANSMITTER);
     * asm volatile ("jmp %0" :: "i" (APP_END+2));
     */
    unsigned char recoveryData[] = {
        0x11, 0x24, 0xf8, 0x94, 0xa8, 0x95, 0x14, 0xbe,
        0xe0, 0xe6, 0xf0, 0xe0, 0x80, 0x81, 0x88, 0x61,
        0x80, 0x83, 0x10, 0x82, 0xe0, 0xec, 0xf0, 0xe0,
        0x80, 0x81, 0x82, 0x60, 0x80, 0x83, 0x80, 0xe1,
        0x80, 0x93, 0xc4, 0x00, 0x88, 0xe1, 0x80, 0x93,
        0xc1, 0x00, 0x0d, 0x94, 0x01, 0xf0,
    };
    memset(pageData, 0xFF, sizeof(pageData));
    memcpy(pageData, recoveryData, sizeof(recoveryData));
    setStatus("Writing recovery page");
    protocol->service().writePage(BOOT_START_ADDR+256, pageData);

    /* Proceed to write out all the data, starting at the last page, ending at the second */
    quint32 pageAddress = BOOT_TOTAL_MEM - 256;
    do {
        int dataAddr = (pageAddress - BOOT_START_ADDR);
        QString status("Writing firmware data: ");
        status += QString::number(BOOT_SIZE - dataAddr);
        status += " / ";
        status += QString::number(BOOT_SIZE);
        status += " bytes";
        setStatus(status);
        setProgress((double) (BOOT_SIZE - dataAddr) / BOOT_SIZE);

        /* Compose the data for this page, padded with 0xFFFF */
        int len = std::max(0, std::min(256, firmwareData.length() - dataAddr));
        memset(pageData+len, 0xFF, 256-len);
        memcpy(pageData, firmwareData.data() + dataAddr, len);
        protocol->service().writePage(pageAddress, pageData);

        pageAddress -= 256;
    } while (pageAddress >= BOOT_START_ADDR);

    /* All done! Switch back to STK500 mode */
    protocol->service().end();
}
