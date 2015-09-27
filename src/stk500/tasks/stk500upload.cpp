#include "../stk500task.h"

void stk500Upload::init() {
    setUsesFirmware(!data.hasFirmwareData());
}

void stk500Upload::run() {
    /* Page buffer used while reading / writing */
    char pageData[256];

    /* When programming both sketch and firmware data; use half progress */
    bool halfProgress = (data.hasFirmwareData() && data.hasSketchData());

    /* Program firmware data using service routine */
    if (data.hasFirmwareData()) {

        /* Verify firmware data */
        if (!data.hasServiceSupport()) {
            throw ProtocolException("The firmware in the hex file selected is incompatible "
                                    "with service-mode programming. To use this firmware, "
                                    "an ISP programmer is required.");
        }

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
            if (halfProgress) {
                setProgress(0.5 * ((double) (BOOT_SIZE - dataAddr) / BOOT_SIZE));
            } else {
                setProgress((double) (BOOT_SIZE - dataAddr) / BOOT_SIZE);
            }

            /* Switch mode for the first page */
            if (pageAddress == BOOT_START_ADDR) {
                protocol->service().setMode('1');
            }

            /* Compose the data for this page, padded with 0xFFFF */
            int len = std::max(0, std::min(256, (int) data.firmwareSize() - dataAddr));
            memset(pageData+len, 0xFF, 256-len);
            memcpy(pageData, data.firmwarePage(dataAddr), len);
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

    /* Program sketch data using STK500 protocol */
    if (data.hasSketchData()) {
        quint32 pageAddress = 0;
        quint32 programSize = data.sketchSize();
        do {
            /* Update status */
            QString status("Writing sketch data: ");
            status += QString::number(pageAddress);
            status += " / ";
            status += QString::number((int) programSize);
            setStatus(status);
            if (halfProgress) {
                setProgress(0.5 + 0.5 * ((double) pageAddress / (double) programSize));
            } else {
                setProgress((double) pageAddress / (double) programSize);
            }

            /* Prepare and write the page of data */
            int len = std::min(256, (int) (programSize - pageAddress));
            memcpy(pageData, data.sketchPage(pageAddress), len);
            memset(pageData+len, 0xFF, 256-len);
            protocol->FLASH_writePage(pageAddress, pageData, 256);

            /* Next page */
            pageAddress += 256;

        } while ((pageAddress < programSize) && !isCancelled());
    }
}
