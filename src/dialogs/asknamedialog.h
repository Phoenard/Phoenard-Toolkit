#ifndef ASKNAMEDIALOG_H
#define ASKNAMEDIALOG_H

#include <QDialog>
#include <QPushButton>

namespace Ui {
class AskNameDialog;
}

class AskNameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AskNameDialog(QWidget *parent = 0);
    ~AskNameDialog();
    void setHelpTitle(const QString &title);
    QString name();
    void setName(const QString &name);
    void setMaxLength(int maxLength);

private slots:
    void on_lineEdit_textChanged(const QString &arg1);

private:
    Ui::AskNameDialog *ui;
};

#endif // ASKNAMEDIALOG_H
