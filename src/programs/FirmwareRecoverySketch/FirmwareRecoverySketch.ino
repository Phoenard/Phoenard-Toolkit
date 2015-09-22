/**************************************************************************
 ******************** Firmware Recovery Sketch ****************************
 **************************************************************************
 *
 * This is a program run as an uploadable sketch that uses cross-calls
 * with the firmware to program the first two pages of bootloader flash.
 * Using instructions given in assembly it is capable of programming
 * from the flash application area by calling code in the bootloader area.
 * 
 * This sketch runs immediately and puts the device into service mode,
 * so the toolkit can continue writing new firmware flash to the device.
 * Do not upload if you can not run the toolkit; you risk locking out.
 * 
 * This program is only supported to run on the ATMEGA2560 running the
 * phoenboot bootloader.
 * 
 * Author: Irmo van den Berge, Phoenard Team
 * Date: 22 September 2015
 */
#include <avr/boot.h>

/* Main service routine data */
const unsigned int service_routine_data[256] = {
  0xC07F, 0x9A27, 0x2CE1, 0xE0C0, 0xE0D0, 0x2CF1, 0xE080, 0xE090, 
  0x01DC, 0x24DD, 0x94D3, 0xE045, 0x2EC4, 0xE053, 0x2EB5, 0x9120, 
  0x00C0, 0xFF27, 0xCFFC, 0x9140, 0x00C6, 0xE621, 0x12E2, 0xC00A, 
  0x2F89, 0x2F9A, 0x2FAB, 0x27BB, 0x2BB4, 0x94F3, 0xE024, 0x12F2, 
  0xC04E, 0xC04C, 0xE727, 0x12E2, 0xC037, 0x2FCD, 0x27DD, 0x2BD4, 
  0x018C, 0x019D, 0x5E10, 0x4023, 0x0931, 0x3F0F, 0x0511, 0x0521, 
  0x0531, 0xF009, 0xF408, 0xC034, 0x10F1, 0xC009, 0x01FC, 0x93A0, 
  0x005B, 0x92B0, 0x0057, 0x95E8, 0xB607, 0xFC00, 0xCFFD, 0xFEF0, 
  0xC027, 0x018C, 0x019D, 0x0D0F, 0x1D11, 0x1D21, 0x1D31, 0x010E, 
  0x01F8, 0x9320, 0x005B, 0x92D0, 0x0057, 0x95E8, 0x2411, 0xEF2F, 
  0x12F2, 0xC016, 0x01FC, 0x93A0, 0x005B, 0x92C0, 0x0057, 0x95E8, 
  0xB607, 0xFC00, 0xCFFD, 0xC00C, 0xE722, 0x12E2, 0xC00C, 0x01AC, 
  0x01BD, 0x0D4F, 0x1D51, 0x1D61, 0x1D71, 0xBF6B, 0x01FA, 0x9147, 
  0x94F3, 0xF021, 0xC004, 0x2EE4, 0x2CF1, 0xC001, 0x2CE1, 0x9340, 
  0x00C6, 0x9120, 0x00C0, 0xFF25, 0xCFFC, 0xCF99, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/* Recovery page: initializes UART/Watchdog and jumps into program */
const unsigned int recovery_section_data[256] = {
  0x2411, 0x94f8, 0x95a8, 0xbe14, 0xe6e0, 0xe0f0, 0x8180, 0x6188, 
  0x8380, 0x8210, 0xece0, 0xe0f0, 0x8180, 0x6082, 0x8380, 0xe180, 
  0x9380, 0x00c4, 0xe188, 0x9380, 0x00c1, 0x940d, 0xf001, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

int main() {
  // Initialize Serial at 115200 baud
  UCSR0A |=  (1 <<U2X0);
  UCSR0B  =  (1 << RXEN0) | (1 << TXEN0);
  UBRR0L  =  0x10;
  UBRR0H  =  0x00;

  // Write first two pages
  printText("Begin Service Recovery\n");
  writePage(0x3e000, service_routine_data);
  writePage(0x3e100, recovery_section_data);
  printText("SUCCESS\n");

  // Clear interrupts and jump to the bootloader
  asm volatile(
    "cli     \n\t" \
    "jmp %0" :: "i" (0x3e002)
  );

  // we never get to this point
  for (;;);
}

void writePage(uint32_t address, const uint16_t* data) {
  // Log
  printText("Writing page ");
  printHex(address, 6);
  printText("...\n");

  // Erase the page
  spm(address, 0x03);

  // Fill buffer with data
  uint8_t i = 0;
  do {
    asm volatile("push r0");
    asm volatile("push r1");
    asm volatile("movw r0, %A0" :: "r" (data[i/2]));
    spm(i, 0x01);
    asm volatile("pop r1");
    asm volatile("pop r0");
  } while (i += 2);

  // Write the page
  spm(address, 0x05);

  // Done
  printText("Finished writing.\n");
}

void spm(uint32_t address, uint8_t cmd) {
  /*
   * Locate the accessible spm instruction in the bootloader area. This
   * is at the end of the flash_write_page function. This looks like this:
   * =========================================
   * 3f3bc: 80 93 5b 00   sts 0x005B, r24
   * 3f3c0: 20 93 57 00   sts 0x0057, r18
   * 3f3c4: e8 95         spm
   * 3f3c6: df 91         pop r29
   * 3f3c8: cf 91         pop r28
   * 3f3ca: 1f 91         pop r17
   * 3f3cc: 0f 91         pop r16
   * 3f3ce: ff 90         pop r15
   * 3f3d0: 08 95         ret
   * =========================================
   */
  static uint32_t spm_addr = 0;
  if (spm_addr == 0) {
    printText("Searching SPM bootloader section...\n");
    static const uint16_t spm_func_data[] = {
      0x9380, 0x005b, 0x9320, 0x0057, 0x95e8, 
      0x91df, 0x91cf, 0x911f, 0x910f, 0x90ff, 0x9508
    };
    static const uint16_t spm_func_len = sizeof(spm_func_data) / sizeof(uint16_t);
    spm_addr = 0x3e000;
    uint8_t found = 0;
    do {
      spm_addr += 2;
      if (spm_addr == 0x40000) {
        printText("SPM bootloader section not found\nERROR\n");
        for(;;);
      }
      
      uint16_t idx = 0;
      do {
        uint16_t word_read = pgm_read_word_far(spm_addr+idx*2);
        uint16_t word_expected = spm_func_data[idx];
        if (word_read != word_expected) break;
        found = (++idx == spm_func_len);
      } while (!found);
    } while (!found);

    spm_addr /= 2;

    printText("Found SPM bootloader section at ");
    printHex(spm_addr, 6);
    printText("\n");
  }

  // Execute the SPM function in the firmware while keeping stack safe
  asm volatile(                 \
    "begin:               \n\t" \
    "  rjmp storepc       \n\t" \
    "program:             \n\t" \
    "  push r15           \n\t" \
    "  push r16           \n\t" \
    "  push r17           \n\t" \
    "  push r28           \n\t" \
    "  push r29           \n\t" \
    "  lds r28, %0        \n\t" \
    "  push r28           \n\t" \
    "  lds r28, %1        \n\t" \
    "  push r28           \n\t" \
    "  lds r28, %2        \n\t" \
    "  push r28           \n\t" \
    "  movw r30, %A3      \n\t" \
    "  movw r24, %C3      \n\t" \
    "  mov r18, %A4       \n\t" \
    "  cli                \n\t" \
    "  ret                \n\t" \
    "storepc:             \n\t" \
    "  rcall program      \n\t" \
    "  sei                \n\t" \
    :                           \
    : "i" ((char*)&spm_addr+0), \
      "i" ((char*)&spm_addr+1), \
      "i" ((char*)&spm_addr+2), \
      "r" (address),            \
      "r" (cmd)                 \
  );

  // Wait for the operation to complete
  boot_spm_busy_wait();
}

void printHex(uint32_t value, int lenPadding) {
  char buff[20];
  ltoa(value, buff, 16);
  int len = strlen(buff);
  printText("(0x");
  while (len != lenPadding) {
    len++;
    printText("0");
  }
  printText(buff);
  printText(")");
}

void printText(const char* text) {
  while (*text) {
    UDR0 = *(text++);
    while (!(UCSR0A & (1 << UDRE0)));
  }
}
