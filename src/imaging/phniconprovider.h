#ifndef PHNICONPROVIDER_H
#define PHNICONPROVIDER_H

#include <QMap>
#include <QIcon>
#include <QString>
#include <QFileIconProvider>
#include "../stk500/stk500.h"

class PHNIconProvider
{
public:
    PHNIconProvider();
    QIcon getIcon(const QString &filePath);
    QIcon getFolderIcon();
    QIcon getDriveIcon();

private:
    QFileIconProvider fileProvider;
    QMap<QString, QIcon> icons;
};

#endif // PHNICONPROVIDER_H
