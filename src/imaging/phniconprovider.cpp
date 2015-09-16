#include "phniconprovider.h"

PHNIconProvider::PHNIconProvider()
{
    fileProvider.setOptions(QFileIconProvider::DontUseCustomDirectoryIcons);
}

QIcon PHNIconProvider::getIcon(const QString &filePath) {
    // Extract file extension from the file path
    QString extension = stk500::getFileExt(filePath).toLower();

    // Look up the icon in the icon map
    QIcon icon = icons[extension];

    // If not yet stored, generate the icon
    if (icon.isNull()) {

        if (extension == ".hex") {
            // Default 32x32 icon for HEX files
            icon = QIcon(":/icons/sketchhex.png");
        } else if (extension == ".ski") {
            // Default 32x32 icon for SKI files
            icon = QIcon(":/icons/sketchski.png");
        } else if (extension == ".lcd") {
            // Default 32x32 icon for LCD files
            icon = QIcon(":/icons/filelcd.png");
        } else {
            // Create a temporary file with this extension and get the icon
            // This completely relies on icons generated from file names
            // It might fail on Linux OS; an alternative might be needed
            QString tmpFilePath = stk500::getTempFile("tmp" + extension);
            QFile tmpFile(tmpFilePath);
            if (tmpFile.open(QFile::WriteOnly)) {
                tmpFile.close();

                icon = fileProvider.icon(QFileInfo(tmpFile));
            } else {
                icon = fileProvider.icon(QFileIconProvider::File);
            }
        }

        // Generate a temporary file with this extension
        icons[extension] = icon;
    }

    return icon;
}

QIcon PHNIconProvider::getFolderIcon() {
    return fileProvider.icon(QFileIconProvider::Folder);
}

QIcon PHNIconProvider::getDriveIcon() {
    return fileProvider.icon(QFileIconProvider::Drive);
}
