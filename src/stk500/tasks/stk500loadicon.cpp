#include "../stk500task.h"

void stk500LoadIcon::run() {
    if (sketch.iconBlock == 0) {
        sketch.setIcon((const char*) SKETCH_DEFAULT_ICON);
    } else {
        sketch.setIcon(protocol->sd().cacheBlock(sketch.iconBlock, true, false, false));
    }
    sketch.iconDirty = false;
    sketch.hasIcon = true;
}
