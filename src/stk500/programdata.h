#ifndef PROGRAMDATA_H
#define PROGRAMDATA_H

#include <QByteArray>
#include <QFile>
#include <QDebug>

class ProgramData
{
public:
    ProgramData();
    void loadFile(const QString &fileName);
    void load(const QByteArray &data);
    bool hasSketchData() { return !_sketchData.isEmpty(); }
    bool hasFirmwareData() { return !_firmwareData.isEmpty(); }
    const QByteArray &sketchData() { return _sketchData; }
    const QByteArray &firmwareData() { return _firmwareData; }
    QString firmwareVersion();

private:
    QByteArray _firmwareData;
    QByteArray _sketchData;
};

#endif // PROGRAMDATA_H
