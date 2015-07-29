//**** ATMEL AVR - A P P L I C A T I O N   N O T E  ************************
//*
//* Title:		AVR068 - STK500 Communication Protocol
//* Filename:		command.h
//* Version:		1.0
//* Last updated:	31.01.2005
//*
//* Support E-mail:	avr@atmel.com
//*
//**************************************************************************

// *****************[ ST State message constants ]***********************

const unsigned char ST_START        = 0;
const unsigned char ST_GET_SEQ_NUM  = 1;
const unsigned char ST_MSG_SIZE_1   = 2;
const unsigned char ST_MSG_SIZE_2   = 3;
const unsigned char ST_GET_TOKEN    = 4;
const unsigned char ST_GET_CMD      = 5;
const unsigned char ST_GET_STATUS   = 6;
const unsigned char ST_GET_DATA     = 7;
const unsigned char ST_GET_CHECK    = 8;
const unsigned char ST_PROCESS      = 9;

// *****************[ STK message constants ]***************************

const unsigned char MESSAGE_START                        = 0x1B;        //= ESC = 27 decimal
const unsigned char  TOKEN                               = 0x0E;

// *****************[ STK Commands enumeration ]************************

enum STK_CMD : unsigned char {

    // *****************[ STK general commands ]*****************
    SIGN_ON                         = 0x01,
    SET_PARAMETER                   = 0x02,
    GET_PARAMETER                   = 0x03,
    SET_DEVICE_PARAMETERS           = 0x04,
    OSCCAL                          = 0x05,
    LOAD_ADDRESS                    = 0x06,
    FIRMWARE_UPGRADE                = 0x07,


    // *****************[ STK ISP commands ]*****************
    ENTER_PROGMODE_ISP              = 0x10,
    LEAVE_PROGMODE_ISP              = 0x11,
    CHIP_ERASE_ISP                  = 0x12,
    PROGRAM_FLASH_ISP               = 0x13,
    READ_FLASH_ISP                  = 0x14,
    PROGRAM_EEPROM_ISP              = 0x15,
    READ_EEPROM_ISP                 = 0x16,
    PROGRAM_FUSE_ISP                = 0x17,
    READ_FUSE_ISP                   = 0x18,
    PROGRAM_LOCK_ISP                = 0x19,
    READ_LOCK_ISP                   = 0x1A,
    READ_SIGNATURE_ISP              = 0x1B,
    READ_OSCCAL_ISP                 = 0x1C,
    SPI_MULTI                       = 0x1D,

    // *****************[ STK PP commands ]*****************
    ENTER_PROGMODE_PP               = 0x20,
    LEAVE_PROGMODE_PP               = 0x21,
    CHIP_ERASE_PP                   = 0x22,
    PROGRAM_FLASH_PP                = 0x23,
    READ_FLASH_PP                   = 0x24,
    PROGRAM_EEPROM_PP               = 0x25,
    READ_EEPROM_PP                  = 0x26,
    PROGRAM_FUSE_PP                 = 0x27,
    READ_FUSE_PP                    = 0x28,
    PROGRAM_LOCK_PP                 = 0x29,
    READ_LOCK_PP                    = 0x2A,
    READ_SIGNATURE_PP               = 0x2B,
    READ_OSCCAL_PP                  = 0x2C,
    SET_CONTROL_STACK               = 0x2D,

    // *****************[ STK HVSP commands ]*****************
    ENTER_PROGMODE_HVSP             = 0x30,
    LEAVE_PROGMODE_HVSP             = 0x31,
    CHIP_ERASE_HVSP                 = 0x32,
    PROGRAM_FLASH_HVSP              = 0x33,
    READ_FLASH_HVSP                 = 0x34,
    PROGRAM_EEPROM_HVSP             = 0x35,
    READ_EEPROM_HVSP                = 0x36,
    PROGRAM_FUSE_HVSP               = 0x37,
    READ_FUSE_HVSP                  = 0x38,
    PROGRAM_LOCK_HVSP               = 0x39,
    READ_LOCK_HVSP                  = 0x3A,
    READ_SIGNATURE_HVSP             = 0x3B,
    READ_OSCCAL_HVSP                = 0x3C,

    // ***************[ Phoenboot commands ]*****************
    READ_RAM_BYTE_ISP               = 0xE0,
    PROGRAM_RAM_BYTE_ISP            = 0xE1,
    READ_RAM_ISP                    = 0xE2,
    PROGRAM_RAM_ISP                 = 0xE3,
    INIT_SD_ISP                     = 0xE6,
    PROGRAM_SD_ISP                  = 0xE7,
    READ_SD_ISP                     = 0xE8,
    PROGRAM_SD_FAT_ISP              = 0xE9,
    READ_ANALOG_ISP                 = 0xEA,
};

// *****************[ STK status constants ]***************************

// Success
const unsigned char  STATUS_CMD_OK                       = 0x00;

// Warnings
const unsigned char  STATUS_CMD_TOUT                     = 0x80;
const unsigned char  STATUS_RDY_BSY_TOUT                 = 0x81;
const unsigned char  STATUS_SET_PARAM_MISSING            = 0x82;

// Errors
const unsigned char  STATUS_CMD_FAILED                   = 0xC0;
const unsigned char  STATUS_CKSUM_ERROR                  = 0xC1;
const unsigned char  STATUS_CMD_UNKNOWN                  = 0xC9;

// *****************[ STK parameter constants ]***************************
const unsigned char  PARAM_BUILD_NUMBER_LOW              = 0x80;
const unsigned char  PARAM_BUILD_NUMBER_HIGH             = 0x81;
const unsigned char  PARAM_HW_VER                        = 0x90;
const unsigned char  PARAM_SW_MAJOR                      = 0x91;
const unsigned char  PARAM_SW_MINOR                      = 0x92;
const unsigned char  PARAM_VTARGET                       = 0x94;
const unsigned char  PARAM_VADJUST                       = 0x95;
const unsigned char  PARAM_OSC_PSCALE                    = 0x96;
const unsigned char  PARAM_OSC_CMATCH                    = 0x97;
const unsigned char  PARAM_SCK_DURATION                  = 0x98;
const unsigned char  PARAM_TOPCARD_DETECT                = 0x9A;
const unsigned char  PARAM_STATUS                        = 0x9C;
const unsigned char  PARAM_DATA                          = 0x9D;
const unsigned char  PARAM_RESET_POLARITY                = 0x9E;
const unsigned char  PARAM_CONTROLLER_INIT               = 0x9F;

// *****************[ STK answer constants ]***************************

const unsigned char  ANSWER_CKSUM_ERROR                  = 0xB0;
