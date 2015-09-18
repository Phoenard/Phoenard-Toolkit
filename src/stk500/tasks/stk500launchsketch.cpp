#include "../stk500task.h"

void stk500LaunchSketch::run() {
    PHN_Settings settings = protocol->readSettings();
    if (settings.getCurrent() != sketchName) {
        settings.setToload(sketchName);
        settings.flags |= SETTINGS_LOAD;
        protocol->writeSettings(settings);
        protocol->resetFirmware();
    }
}
