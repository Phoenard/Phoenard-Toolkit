#ifndef STK500_FAT_H
#define STK500_FAT_H

#include <QString>

/* End Of Chain values for FAT entries */
/** FAT16 end of chain value used by Microsoft. */
const quint32 FAT16EOC = 0XFFFF;
/** Minimum value for FAT16 EOC.  Use to test for EOC. */
const quint32 FAT16EOC_MIN = 0XFFF8;
/** FAT32 end of chain value used by Microsoft. */
const quint32 FAT32EOC = 0X0FFFFFFF;
/** Minimum value for FAT32 EOC.  Use to test for EOC. */
const quint32 FAT32EOC_MIN = 0X0FFFFFF8;
/** Mask a for FAT32 entry. Entries are 28 bits. */
const quint32 FAT32MASK = 0X0FFFFFFF;

/** name[0] value for entry that is free after being "deleted" */
const quint8 DIR_NAME_DELETED = 0XE5;
/** name[0] value for entry that is free and no allocated entries follow */
const quint8 DIR_NAME_FREE = 0X00;

/** Directory entry is a system file */
const quint8 DIR_ATT_SYSTEM = 0x4;
/** Directory entry is hidden */
const quint8 DIR_ATT_HIDDEN = 0x2;
/** file is read-only */
const quint8 DIR_ATT_READ_ONLY = 0X01;
/** Directory entry contains the volume label */
const quint8 DIR_ATT_VOLUME_ID = 0X08;
/** Entry is for a directory */
const quint8 DIR_ATT_DIRECTORY = 0X10;
/** Mask for file/subdirectory tests */
const quint8 DIR_ATT_FILE_TYPE_MASK = (DIR_ATT_VOLUME_ID | DIR_ATT_DIRECTORY);
/** Mask for LFN (long file name) entries */
const quint8 DIR_ATT_LFN = (DIR_ATT_VOLUME_ID | DIR_ATT_HIDDEN | DIR_ATT_READ_ONLY | DIR_ATT_SYSTEM);

const quint32 CLUSTER_EOC = 0x0FFFFFFF;

/* stores all information about a loaded volume */
typedef struct {
  quint8 isInitialized;     /* Indicates whether the volume is successfully initialized. Reset when errors occur */
  quint8 isMultiFat;        /* More than one FAT on the volume */
  quint8 isfat16;           /* volume type is FAT16, FAT32 otherwise */
  quint8 blocksPerCluster;  /* cluster size in blocks */
  quint32 blocksPerFat;     /* FAT size in blocks */
  quint32 clusterLast;      /* the index of the first cluster that is out of range */
  quint32 rootCluster;      /* First cluster of a FAT32 root directory, for FAT16: root offset */
  quint32 rootSize;         /* Total size of a FAT16 root directory */
  quint32 dataStartBlock;   /* first data block number */
  quint32 fatStartBlock;    /* start block for first FAT */
} CardVolume;

/**
 * \struct directoryEntry
 * \brief FAT short directory entry
 *
 * Short means short 8.3 name, not the entry size.
 *
 * Date Format. A FAT directory entry date stamp is a 16-bit field that is
 * basically a date relative to the MS-DOS epoch of 01/01/1980. Here is the
 * format (bit 0 is the LSB of the 16-bit word, bit 15 is the MSB of the
 * 16-bit word):
 *
 * Bits 9-15: Count of years from 1980, valid value range 0-127
 * inclusive (1980-2107).
 *
 * Bits 5-8: Month of year, 1 = January, valid value range 1-12 inclusive.
 *
 * Bits 0-4: Day of month, valid value range 1-31 inclusive.
 *
 * Time Format. A FAT directory entry time stamp is a 16-bit field that has
 * a granularity of 2 seconds. Here is the format (bit 0 is the LSB of the
 * 16-bit word, bit 15 is the MSB of the 16-bit word).
 *
 * Bits 11-15: Hours, valid value range 0-23 inclusive.
 *
 * Bits 5-10: Minutes, valid value range 0-59 inclusive.
 *
 * Bits 0-4: 2-second count, valid value range 0-29 inclusive (0 - 58 seconds).
 *
 * The valid time range is from Midnight 00:00:00 to 23:59:58.
 */
typedef struct {
           /**
            * Short 8.3 name.
            * The first eight bytes contain the file name with blank fill.
            * The last three bytes contain the file extension with blank fill.
            */
  quint8  name_raw[11];
          /** Entry attributes.
           *
           * The upper two bits of the attribute byte are reserved and should
           * always be set to 0 when a file is created and never modified or
           * looked at after that.  See defines that begin with DIR_ATT_.
           */
  quint8  attributes;
          /**
           * Reserved for use by Windows NT. Set value to 0 when a file is
           * created and never modify or look at it after that.
           */
  quint8  reservedNT;
          /**
           * The granularity of the seconds part of creationTime is 2 seconds
           * so this field is a count of tenths of a second and its valid
           * value range is 0-199 inclusive. (WHG note - seems to be hundredths)
           */
  quint8  creationTimeTenths;
           /** Time file was created. */
  quint16 creationTime;
           /** Date file was created. */
  quint16 creationDate;
          /**
           * Last access date. Note that there is no last access time, only
           * a date.  This is the date of last read or write. In the case of
           * a write, this should be set to the same date as lastWriteDate.
           */
  quint16 lastAccessDate;
          /**
           * High word of this entry's first cluster number (always 0 for a
           * FAT12 or FAT16 volume).
           */
  quint16 firstClusterHigh;
           /** Time of last write. File creation is considered a write. */
  quint16 lastWriteTime;
           /** Date of last write. File creation is considered a write. */
  quint16 lastWriteDate;
           /** Low word of this entry's first cluster number. */
  quint16 firstClusterLow;
           /** 32-bit unsigned holding this file's size in bytes. */
  quint32 fileSize;

  /* Helper functions */
  const bool isFile() { return (attributes & DIR_ATT_FILE_TYPE_MASK) == 0; }
  const bool isDirectory() { return (attributes & DIR_ATT_DIRECTORY) == DIR_ATT_DIRECTORY; }
  const bool isVolume() { return (attributes & DIR_ATT_VOLUME_ID) == DIR_ATT_VOLUME_ID; }
  const bool isReadOnly() { return (attributes & DIR_ATT_READ_ONLY) == DIR_ATT_READ_ONLY; }
  const bool isLFN() { return (attributes & DIR_ATT_LFN) == DIR_ATT_LFN; }
  const bool isFree() { return name_raw[0] == DIR_NAME_FREE; }
  const bool isDeleted() { return name_raw[0] == DIR_NAME_DELETED; }

  const bool LFN_isDeleted() { return (((unsigned char*) this)[0] & 0x40) == 0x80; }
  const bool LFN_isLast() { return (((unsigned char*) this)[0] & 0x40) == 0x40; }
  const unsigned char LFN_ordinal() { return (((unsigned char*) this)[0] & 0x3F) - 1; }
  const unsigned char LFN_checksum() { return ((unsigned char*) this)[13]; }

  void LFN_setOrdinal(unsigned char ordinal, bool isLast) {
      ordinal++;
      if (isLast) ordinal |= 0x40;
      ((unsigned char*) this)[0] = ordinal;
  }

  void LFN_setChecksum(unsigned char checksum) {
      ((unsigned char*) this)[13] = checksum;
  }

  // Gets the long file name stored within this Entry
  void LFN_readName(ushort* dest, int index) {
      char* data = (char*) this;
      ushort* outBuff = dest + (index * 13);

      /* Always contains 13 unicode characters; store 26 bytes in a buffer */
      memcpy(outBuff + 0, data + 1, 5 * sizeof(ushort));
      memcpy(outBuff + 5, data + 14, 6 * sizeof(ushort));
      memcpy(outBuff + 11, data + 28, 2 * sizeof(ushort));
  }
  // Write the long file name to this entry
  void LFN_writeName(const ushort* src, int index) {
      char* data = (char*) this;
      const ushort* inBuff = src + (index * 13);

      /* Always contains 13 unicode characters; store 26 bytes in a buffer */
      memcpy(data + 1, inBuff + 0, 5 * sizeof(ushort));
      memcpy(data + 14, inBuff + 5, 6 * sizeof(ushort));
      memcpy(data + 28, inBuff + 11, 2 * sizeof(ushort));
  }

  QString LFN_name() {
      /* Read name */
      ushort outBuff[13];
      LFN_readName(outBuff, 0);

      /* Convert buffered data to UTF16 String from NULL-terminated unicode array */
      int len = 0;
      for (;len < 13 && outBuff[len]; len++);
      return QString::fromUtf16(outBuff, len);
  }

  quint32 firstCluster() {
      return (quint32) (firstClusterHigh << 16) | (quint32) firstClusterLow;
  }

  void setFirstCluster(quint32 firstCluster) {
      firstClusterLow = (quint16) (firstCluster & 0XFFFF);
      firstClusterHigh = (quint16) (firstCluster >> 16);
  }

  unsigned char name_crc() {
      unsigned char crc = 0;
      for (char i=0; i<11; i++)
        crc = ((crc<<7) | (crc>>1)) + name_raw[i];
      return crc;
  }

  QString name() {
      if (isVolume()) {
          /* Different kind of calculation for volume names */
          int name_cnt = 11;
          while (name_cnt > 0 && name_raw[name_cnt - 1] == ' ') {
              name_cnt--;
          }
          return QString::fromLocal8Bit((char*) name_raw, name_cnt);
      } else {
          /* Count how many spaces are appended after the file name */
          int name_cnt = 8;
          while (name_cnt > 0 && name_raw[name_cnt - 1] == ' ') {
              name_cnt--;
          }

          /* Count how many spaces are appended after the extension */
          int ext_cnt = 3;
          while (ext_cnt > 0 && name_raw[ext_cnt - 1 + 8] == ' ') {
              ext_cnt--;
          }

          bool has_dot = (ext_cnt > 0) && (name_cnt > 0);
          int str_len = name_cnt + ext_cnt + (has_dot ? 1 : 0);
          char* str_arr = new char[str_len];
          for (int i = 0; i < name_cnt; i++) {
              str_arr[i] = name_raw[i];
          }
          if (has_dot) {
              str_arr[name_cnt] = '.';
              name_cnt++;
          }
          if (ext_cnt > 0) {
              for (int i = 0; i < ext_cnt; i++) {
                  str_arr[i + name_cnt] = name_raw[i + 8];
              }
          }
          QString rval = QString::fromLocal8Bit(str_arr, str_len);
          delete[] str_arr;
          return rval;
      }
  }

  void setName(QString shortName) {
      if (isVolume()) {
          /* Different kind of calculation for volume names */
          memset(name_raw, ' ', 11);
          for (int i = 0; i < shortName.length() && i < 11; i++) {
              name_raw[i] = shortName.at(i).toLatin1();
          }
      } else {
          int extIdx = shortName.lastIndexOf('.') + 1;
          if (extIdx) {
              // Has an extension, deal with both
              for (int i = 0; i < 8; i++) {
                  if (i >= (extIdx - 1)) {
                      name_raw[i] = ' ';
                  } else {
                      name_raw[i] = shortName.at(i).toLatin1();
                  }
              }
              for (int i = 0; i < 3; i++) {
                  int ni = extIdx + i;
                  if (ni >= shortName.length()) {
                      name_raw[8 + i] = ' ';
                  } else {
                      name_raw[8 + i] =  shortName.at(ni).toLatin1();
                  }
              }
          } else {
              // No extension, only take the name limited by 8 chars
              for (int i = 0; i < 11; i++) {
                  if (i >= 8 || i >= shortName.length()) {
                      name_raw[i] = ' ';
                  } else {
                      name_raw[i] = shortName.at(i).toLatin1();
                  }
              }
          }
      }
  }

} DirectoryEntry;

typedef struct DirectoryEntryPtr {
    DirectoryEntryPtr() : block(0), index(0) {}
    DirectoryEntryPtr(quint32 block, quint8 index) : block(block), index(index) {}

    quint32 block;
    quint8 index;

    const bool isValid() { return (block != 0) || (index != 0); }
    inline bool operator==(const DirectoryEntryPtr& ptr) {
        return (ptr.block == block) && (ptr.index == index);
    }
    inline bool operator!=(const DirectoryEntryPtr& ptr) {
        return (ptr.block != block) || (ptr.index != index);
    }
} DirectoryEntryPtr;

#endif // STK500_FAT_H
