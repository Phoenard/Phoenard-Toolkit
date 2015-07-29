#ifndef SERIALMONITORWIDGET_H
#define SERIALMONITORWIDGET_H

#include <QWidget>
#include <QDebug>
#include "stk500/stk500serial.h"
#include "imageeditor.h"

namespace Ui {
class serialmonitorwidget;
}

typedef struct {
    quint16 cur_x;
    quint16 cur_y;
    quint16 entrymode;
    quint16 view_ha;
    quint16 view_hb;
    quint16 view_va;
    quint16 view_vb;
    quint8 cmd;
    quint8 cmd_len;
    quint8 cmd_buff[50];
    quint8 cmd_buff_index;
} ScreenRegisters;

class serialmonitorwidget : public QWidget
{
    Q_OBJECT

public:
    serialmonitorwidget(QWidget *parent = 0);
    ~serialmonitorwidget();
    void setSerial(stk500Serial *serial);
    void openSerial();
    void setScreenShare(bool enabled);
    ImageEditor* getImageEditor();

private:
    void receiveScreen(quint8 byte);
    void receivePixel(quint16 color);
    bool moveCursor_x(qint8 dx);
    bool moveCursor_y(qint8 dy);

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
    ScreenRegisters screen;
    bool screenEnabled;
};

#endif // SERIALMONITORWIDGET_H
