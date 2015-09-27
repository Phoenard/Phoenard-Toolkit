#include "../stk500task.h"

void stk500UpdateFirmware::init() {
    setUsesFirmware(false);
}

void stk500UpdateFirmware::run() {
    /* Page buffer used while reading / writing */
    char pageData[256];

    /* Verify firmware data */


    /* If possible, first read the old sketch program before switching */
    QByteArray oldSketchData;
    if (protocol->state() != STK500::SERVICE) {
        oldSketchData.fill((char) 0xFF, 1024);
        for (quint32 addr = 0; addr < 1024; addr += 256) {
            protocol->FLASH_readPage(addr, oldSketchData.data() + addr, 256);
        }
    }

    /* Initialize service routine */
    setStatus("Initializing service routine");
    protocol->service().begin();

    /* Switch to operative mode 0 */
    setStatus("Switching programming mode");
    protocol->service().setMode('0');

    /* Proceed to write out all the data, starting at the last page, ending at the first */
    quint32 pageAddress = BOOT_TOTAL_MEM - 256;
    do {
        /* Update status and progress */
        int dataAddr = (pageAddress - BOOT_START_ADDR);
        QString status("Writing firmware data: ");
        status += QString::number(BOOT_SIZE - dataAddr);
        status += " / ";
        status += QString::number(BOOT_SIZE);
        status += " bytes";
        setStatus(status);
        setProgress((double) (BOOT_SIZE - dataAddr) / BOOT_SIZE);

        /* Switch mode for the first page */
        if (pageAddress == BOOT_START_ADDR) {
            protocol->service().setMode('1');
        }

        /* Compose the data for this page, padded with 0xFFFF */
        int len = std::max(0, std::min(256, (int) firmwareData.firmwareSize() - dataAddr));
        memset(pageData+len, 0xFF, 256-len);
        memcpy(pageData, firmwareData.firmwarePage(dataAddr), len);
        protocol->service().writePage(pageAddress, pageData);

        pageAddress -= 256;
    } while (pageAddress >= BOOT_START_ADDR);

    /* All done! Switch back to STK500 mode */
    setStatus("Finalizing");
    protocol->service().end();

    /* Restore previous program if available, otherwise wipe first page to disable it */
    if (oldSketchData.isEmpty()) {
        char emptyPageData[256];
        memset(emptyPageData, 0xFF, sizeof(emptyPageData));
        protocol->FLASH_writePage(0, emptyPageData, sizeof(emptyPageData));
    } else {
        for (quint32 addr = 0; addr < 1024; addr += 256) {
            protocol->FLASH_writePage(addr, oldSketchData.data() + addr, 256);
        }
    }
}
