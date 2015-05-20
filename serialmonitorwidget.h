#ifndef SERIALMONITORWIDGET_H
#define SERIALMONITORWIDGET_H

#include <QWidget>
#include <QDebug>
#include "stk500/stk500serial.h"

namespace Ui {
class serialmonitorwidget;
}

class serialmonitorwidget : public QWidget
{
    Q_OBJECT

public:
    serialmonitorwidget(QWidget *parent = 0);
    ~serialmonitorwidget();
    void setSerial(stk500Serial *serial);
    void openSerial();

private slots:
    void readSerialOutput();
    void clearOutputText();

    void on_runSketchCheck_toggled(bool checked);

    void on_baudrateBox_activated(int index);

    void on_messageTxt_returnPressed();

    void on_sendButton_clicked();

private:
    Ui::serialmonitorwidget *ui;
    stk500Serial *serial;
    QTimer *updateTimer;
};

#endif // SERIALMONITORWIDGET_H
