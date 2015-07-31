#ifndef STK500SETTINGS_H
#define STK500SETTINGS_H

// Settings taken over from Phoenard Arduino Library
// Uses Qt datatypes for proper compatibility
#include <Qt>
#include <QString>

/* Flags used for status eeprom values */
/**@name Setting flags
 * @brief The available flags stored inside PHN_Settings
 */
//@{
/// Specifies a sketch has to be loaded from Micro-SD
#define SETTINGS_LOAD           0x01
/// Specifies that loading must be skipped, with program wiped
#define SETTINGS_LOADWIPE       0x02
/// Specifies that the current sketch is modified and needs saving
#define SETTINGS_MODIFIED       0x04
/// Specifies that touch input is inverted horizontally
#define SETTINGS_TOUCH_HOR_INV  0x08
/// Specifies that touch input is inverted vertically
#define SETTINGS_TOUCH_VER_INV  0x10
/// Specifies settings have been changed - never written to EEPROM
#define SETTINGS_CHANGED        0x80
//@}

/// Structure holding the settings stored in EEPROM
typedef struct PHN_Settings {
  quint8 touch_hor_a;
  quint8 touch_hor_b;
  quint8 touch_ver_a;
  quint8 touch_ver_b;
  char sketch_toload[8];
  char sketch_current[8];
  quint32 sketch_size;
  quint8 flags;

  void setToload(QString name);
  QString getToload();
  void setCurrent(QString name);
  QString getCurrent();
} PHN_Settings;

/* Default Phoenard settings, used to restore corrupted or wiped EEPROM */
#define SETTINGS_DEFAULT_SKETCH        {'S', 'K', 'E', 'T', 'C', 'H', 'E', 'S'}
#define SETTINGS_DEFAULT_FLAGS         (SETTINGS_LOAD | SETTINGS_CHANGED | SETTINGS_TOUCH_HOR_INV)
#define SETTINGS_DEFAULT_TOUCH_HOR_A   100
#define SETTINGS_DEFAULT_TOUCH_HOR_B   100
#define SETTINGS_DEFAULT_TOUCH_VER_A   120
#define SETTINGS_DEFAULT_TOUCH_VER_B   120

const PHN_Settings SETTINGS_DEFAULT = {
  SETTINGS_DEFAULT_TOUCH_HOR_A, SETTINGS_DEFAULT_TOUCH_HOR_B,
  SETTINGS_DEFAULT_TOUCH_VER_A, SETTINGS_DEFAULT_TOUCH_VER_B,
  SETTINGS_DEFAULT_SKETCH, SETTINGS_DEFAULT_SKETCH, 0, SETTINGS_DEFAULT_FLAGS
};

/// The total size of the device EEPROM
#define EEPROM_SIZE           4096
/// The total size of the settings stored (note: numeral because of memory alignment)
#define EEPROM_SETTINGS_SIZE  25
/// The address in EEPROM where the settings are stored
#define EEPROM_SETTINGS_ADDR  (EEPROM_SIZE - EEPROM_SETTINGS_SIZE)

#endif // STK500SETTINGS_H
