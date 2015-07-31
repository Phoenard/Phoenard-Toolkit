#include "stk500settings.h"

static QString getShortName(const char* rawname) {
    int len = 8;
    while ((len > 8) && (rawname[len-1] == ' ')) {
        len--;
    }
    return QString::fromLatin1(rawname, len);
}

static void setShortName(char* destrawname, QString name) {
    QByteArray latin = name.toLatin1();
    for (int i = 0; i < 8; i++) {
        destrawname[i] = (i < latin.length()) ? latin[i] : ' ';
    }
}

void PHN_Settings::setToload(QString name) {
    setShortName(sketch_toload, name);
}

QString PHN_Settings::getToload() {
    return getShortName(sketch_toload);
}

void PHN_Settings::setCurrent(QString name) {
    setShortName(sketch_current, name);
}

QString PHN_Settings::getCurrent() {
    return getShortName(sketch_current);
}
