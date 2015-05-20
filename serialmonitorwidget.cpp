#include "serialmonitorwidget.h"
#include "ui_serialmonitorwidget.h"
#include <QScrollBar>

serialmonitorwidget::serialmonitorwidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::serialmonitorwidget)
{
    ui->setupUi(this);

    this->updateTimer = new QTimer(this);
    connect(this->updateTimer, SIGNAL(timeout()), this, SLOT(readSerialOutput()));
    this->updateTimer->start(50);
}

serialmonitorwidget::~serialmonitorwidget()
{
    delete ui;
}

void serialmonitorwidget::setSerial(stk500Serial *serial)
{
    this->serial = serial;

    connect(serial, SIGNAL(serialOpened()),
            this,    SLOT(clearOutputText()),
            Qt::QueuedConnection);
}

void serialmonitorwidget::openSerial()
{
    if (!ui->runSketchCheck->isChecked()) {
        serial->closeSerial();
    } else {
        QString baudSel = ui->baudrateBox->currentText();
        QString baud_pfix = " baud";
        if (baudSel.endsWith(baud_pfix)) {
            baudSel.chop(baud_pfix.length());
        }
        serial->openSerial(baudSel.toInt());
    }
}

/* Re-open Serial in the right mode when running is toggled */
void serialmonitorwidget::on_runSketchCheck_toggled(bool)
{
    openSerial();
}

/* Re-open Serial in the right baud rate when rate is changed */
void serialmonitorwidget::on_baudrateBox_activated(int)
{
    openSerial();
}

/* Redirect enter presses to the send button */
void serialmonitorwidget::on_messageTxt_returnPressed()
{
    ui->sendButton->click();
}

/* Send message to Serial when send button is clicked */
void serialmonitorwidget::on_sendButton_clicked()
{
    QString message = ui->messageTxt->text();
    ui->messageTxt->clear();
    if (message.isEmpty()) {
        return;
    }

    // Append newlines
    int newline_idx = ui->lineEndingBox->currentIndex();
    if (newline_idx == 1 || newline_idx == 3) {
        message.append('\n'); // Newline
    }
    if (newline_idx == 2 || newline_idx == 3) {
        message.append('\r'); // Carriage return
    }

    // Send it to Serial
    serial->write(message);
}

/* Clear output text - when serial opens */
void serialmonitorwidget::clearOutputText() {
    ui->outputText->clear();
}

/* Read Serial output and display it in the text area */
void serialmonitorwidget::readSerialOutput()
{
    char buff[1025];
    int len = serial->read(buff, 1024);
    if (len) {
        buff[len] = 0;
        QString myString(buff);
        myString = myString.remove('\r');

        if (ui->autoScrollCheck->isChecked()) {

            ui->outputText->moveCursor (QTextCursor::End);
            ui->outputText->insertPlainText(myString);
            ui->outputText->moveCursor (QTextCursor::End);
        } else {
            int old_scroll = ui->outputText->verticalScrollBar()->value();
            QTextCursor old_cursor = ui->outputText->textCursor();
            ui->outputText->moveCursor (QTextCursor::End);
            ui->outputText->insertPlainText(myString);
            ui->outputText->setTextCursor(old_cursor);
            ui->outputText->verticalScrollBar()->setValue(old_scroll);
        }
    }
}
