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
    QString firmwareVersion() const;
    bool hasSketchData() const { return !_sketchData.isEmpty(); }
    bool hasFirmwareData() const { return !_firmwareData.isEmpty(); }
    const QByteArray &sketchData() { return _sketchData; }
    const QByteArray &firmwareData() { return _firmwareData; }
    quint32 sketchSize() const { return (quint32) _sketchData.size(); }
    quint32 firmwareSize() const { return (quint32) _firmwareData.size(); }
    const char* sketchPage(quint32 address) const;
    const char* firmwarePage(quint32 address) const;
    bool hasServiceSupport() const;

    /* Verifies if a 256-byte page of data is the service page */
    static bool isServicePage(const char* pageData);

private:
    void pageAlign(QByteArray &data);
    QByteArray _firmwareData;
    QByteArray _sketchData;
};

#endif // PROGRAMDATA_H
