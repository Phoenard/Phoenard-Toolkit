#ifndef CODESELECTDIALOG_H
#define CODESELECTDIALOG_H

#include <QDialog>

namespace Ui {
class CodeSelectDialog;
}

class CodeSelectDialog : public QDialog
{
    Q_OBJECT

    enum Mode { OPEN, SAVE };

public:
    explicit CodeSelectDialog(QWidget *parent = 0);
    ~CodeSelectDialog();
    void setMode(Mode mode);
    void setData(QByteArray &data);
    QByteArray &getData() { return this->data; }

private:
    void detect_settings();
    void apply_settings();

private slots:
    void settings_changed();
    void on_codeEdit_textChanged();


private:
    Ui::CodeSelectDialog *ui;
    Mode mode;
    QByteArray data;
};

#endif // CODESELECTDIALOG_H
