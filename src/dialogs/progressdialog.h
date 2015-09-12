#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QCloseEvent>

namespace Ui {
class ProgressDialog;
}

class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = 0);
    ~ProgressDialog();
    bool isCancelClicked() { return cancelClicked; }
    void updateProgress(double progress, QString status);

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void on_pushButton_clicked();

private:
    Ui::ProgressDialog *ui;
    bool cancelClicked;
};

#endif // PROGRESSDIALOG_H
