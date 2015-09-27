/***********************************************************************************************
 *********************************** Service Mode Sketch ***************************************
 ***********************************************************************************************
 *
 * This is a program run as an uploadable sketch that uses cross-calls with the firmware
 * to program the bootloader area of the ATMEGA2560. Using instructions given in assembly
 * it is capable of programming from the flash application area by calling code in the
 * bootloader area. Updating is done using a very simple UART communication protocol.
 *
 * == Self-service protocol description ==
 * Every command starts with either a (address), r (read page), w (write page) or m (mode)
 * Using these commands the firmware flash memory area can addressed, read and programmed.
 * The first page (the first 256 bytes) of the firmware is protected and reserved for the
 * updater to safely program the others pages of flash. Every byte received from UART is
 * echo'd back for verification. While reading page data, every byte received is responded
 * with the read data at that particular page address and offset. When programming is finished,
 * the first page should be written to complete the firmware and exit service mode.
 *
 * == Lock-out protection ==
 * To prevent complete lock-out during flashing, the service mode (m) command is used to
 * write a programming section and jump-to-sketch in the first page of the firmware.
 * If the device is reset while not all firmware is written, this switches the device back
 * to service mode automatically. While writing firmware the programming section on the
 * first page is used to avoid writing flash that is currently being executed.
 * 
 * == Supported bootloaders ==
 * This update mechanic makes use of a strict function signature that enables it to program
 * flash memory. This function address is computed automatically by reading through the flash
 * memory. This means only bootloaders that expose this particular piece of code are supported.
 *
 * == Giving the bootloader multiple lives ==
 * Different bootloaders can be swapped between easily. Programming from a variety of data
 * sources becomes possible, as well extending the available STK commands for specific needs.
 * 
 * This program is only supported to run on the ATMEGA2560 running the
 * standard phoenboot bootloader.
 * 
 * Created:   05 September 2015
 * Modified:  22 September 2015
 * Author:    Irmo van den Berge, Phoenard Team
 */
#include <avr/boot.h>

/* Bootloader location in flash memory */
#define BOOT_START 0x3e000

/*
 * Assembly data and calculated address for the SPM function we use
 * This function includes a jump to the sketch as a first instruction:
 * ===================================================
 * 3e000: 0c 94 00 00   jmp 0 ; 0x0 <__tmp_reg__>
 * 3e004: 80 93 5b 00   sts 0x005B, r24
 * 3e008: 20 93 57 00   sts 0x0057, r18
 * 3e00C: e8 95         spm
 * 3e00E: df 91         pop r29
 * 3e010: cf 91         pop r28
 * 3e012: 1f 91         pop r17
 * 3e014: 0f 91         pop r16
 * 3e016: ff 90         pop r15
 * 3e018: 08 95         ret
 * ===================================================
 */
static const uint16_t spm_func_data[] = {
  0x940c, 0x0000, 0x9380, 0x005b, 0x9320, 0x0057, 
  0x95e8, 0x91df, 0x91cf, 0x911f, 0x910f, 0x90ff, 0x9508
};
static const uint16_t spm_func_off = 2;
static const uint16_t spm_func_len = sizeof(spm_func_data) / sizeof(uint16_t);
static uint32_t spm_func_addr = 0;
static const uint32_t spm_boot_func_addr = (BOOT_START>>1)+spm_func_off;

int main() {
  // Initialize Serial at 115200 baud
  UCSR0A |=  (1 <<U2X0);
  UCSR0B  =  (1 << RXEN0) | (1 << TXEN0);
  UBRR0L  =  0x10;
  UBRR0H  =  0x00;

  /* Turn on LED13 to notify we are in service mode */
  DDRB |= _BV(7);

  /* Update routine starts here */
  uint32_t addr = 0;
  uint8_t ctr = 0;
  char mode = 0;
  uint16_t pageBuffer[128];
  for (;;) {
    /* Receive next byte from UART; wait forever to get it */
    while (!(UCSR0A & (1 << RXC0)));
    unsigned char c = UDR0;

    if (mode == 'a') {
      /* 'a'-mode: Receive the 32-bit address value */
      addr >>= 8;
      addr |= ((uint32_t) c << 24);
      if (++ctr == 4) mode = 0;

    } else if (mode == 'w') {
      /* 'w'-mode: Write out a single page */
      ((unsigned char*) pageBuffer)[ctr] = c;

      /* Increment counter nad write out the page at the 256th byte */
      if (++ctr == 0) {
        writePage(addr, pageBuffer);
        mode = 0;
      }

    } else if (mode == 'r') {
      /* 'r'-mode: Read next byte of program memory; stop after 256 bytes */
      c = pgm_read_byte_far(addr + ctr);
      if (++ctr == 0) mode = 0;

    } else if (mode == 'm') {  
      /* 
       * Mode 0: Use first page of firmware to execute SPM instructions. If first page
       *         of firmware does not already contain the function, write it first by
       *         using the function existing elsewhere in firmware.
       * 
       * Mode 1: Use function in existing firmware to execute SPM instructions.
       */
      if (ctr == 0) {
        /* Switch modes */
        if (c == '0') {
          if (spmInit(BOOT_START) && (spm_func_addr != spm_boot_func_addr)) {
            writePage(BOOT_START, spm_func_data);
            spm_func_addr = spm_boot_func_addr;
          }
        } else if (c == '1') {
          spmInit(BOOT_START + 256);
        }
      } else {
        /* Respond with current function address */
        c = ((unsigned char*) &spm_func_addr)[ctr-1];
      }

      /* End after 5 sent out bytes */
      if (++ctr == 5) mode = 0;

    } else {
      /* Unknown mode: set new mode */
      mode = c;
      ctr = 0;
    }

    /* Send back every received character for verification and timings */
    UDR0 = c;
    while (!(UCSR0A & (1 << UDRE0)));
  }
}

void writePage(uint32_t address, const uint16_t* data) {
  // Sketch protection: writing to application area locks program
  if (address < BOOT_START) return;

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
}

void spm(uint32_t address, uint8_t cmd) {
  // Don't do anything if not initialized
  if (spm_func_addr == 0) return;

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
    : "i" ((char*)&spm_func_addr+0), \
      "i" ((char*)&spm_func_addr+1), \
      "i" ((char*)&spm_func_addr+2), \
      "r" (address),            \
      "r" (cmd)                 \
  );

  // Wait for the operation to complete
  boot_spm_busy_wait();
}

/*
 * Locate the first accessible spm function in the bootloader area.
 */
uint8_t spmInit(uint32_t startAddress) {
  spm_func_addr = startAddress;
  for (;;) {
    spm_func_addr += 2;
    if (spm_func_addr == 0x40000) {
      spm_func_addr = 0;
      return 0;
    }

    uint16_t idx = spm_func_off;
    uint16_t addr_off = 0;
    for (;;) {
      uint16_t word_read = pgm_read_word_far(spm_func_addr+addr_off);
      uint16_t word_expected = spm_func_data[idx];
      if (word_read != word_expected) break;
      addr_off += 2;
      if (++idx == spm_func_len) {
        spm_func_addr /= 2;
        return 1;
      }
    }
  }
}
