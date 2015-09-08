#include "../stk500task.h"

void stk500ListSketches::run() {
    /* Read the currently loaded sketch from EEPROM */
    currentSketch = protocol->readSettings().getCurrent();

    /* List all file entries in the root directory */
    QList<DirectoryInfo> rootEntries = sd_list(protocol->sd().getDirPtrFromCluster(0));
    if (isCancelled()) return;

    /* Go by all entries and find all icons, or use the defaults */
    for (int entryIdx = 0; entryIdx < rootEntries.length(); entryIdx++) {
        DirectoryInfo &info = rootEntries[entryIdx];
        DirectoryEntry entry = info.entry();
        QString shortName = info.shortName();
        QString shortExt = stk500::getFileExt(shortName);
        bool isIcon = (shortExt == ".SKI");

        /* Update current progress */
        setProgress((double) entryIdx / (double) rootEntries.length());

        /* Check if this entry is a HEX or SKI file */
        if (info.isDirectory() || (!isIcon && (shortExt != ".HEX"))) {
            continue;
        }

        /* Generate a temporary entry for comparison and adding */
        SketchInfo sketch;
        sketch.name = stk500::trimFileExt(info.shortName());
        sketch.fullName = stk500::trimFileExt(info.name());
        sketch.hasIcon = false;
        sketch.iconDirty = true;
        sketch.iconBlock = 0;

        /* Locate this entry in the current results, or add if not found */
        int sketchIndex = -1;
        for (int i = 0; i < sketches.length(); i++) {
            SketchInfo &other = sketches[i];
            if (sketch.name == other.name) {
                sketchIndex = i;
                sketch = other;
                break;
            }
        }
        if (sketchIndex == -1) {
            sketchIndex = sketch.index = sketches.length();
            sketches.append(sketch);
        }

        /* If icon, load the icon data */
        quint32 firstCluster = entry.firstCluster();
        if (isIcon) {
            if (firstCluster == 0) {
                sketch.iconBlock = 0;
            } else {
                sketch.iconBlock = protocol->sd().getClusterBlock(firstCluster);
            }
        }

        /* Update entry */
        sketches[sketchIndex] = sketch;
    }

    /* Completed */
    setProgress(1.0);
}