#include "asknamedialog.h"
#include "ui_asknamedialog.h"

AskNameDialog::AskNameDialog(QWidget *parent) :
    QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
    ui(new Ui::AskNameDialog)
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    this->setFixedSize(this->size());
}

AskNameDialog::~AskNameDialog()
{
    delete ui;
}

void AskNameDialog::setHelpTitle(const QString &title) {
    ui->helpLabel->setText(title);
}

void AskNameDialog::on_lineEdit_textChanged(const QString &text)
{
    bool isValid = (text.length() > 0);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid);
}


QString AskNameDialog::name() {
    return ui->lineEdit->text();
}

void AskNameDialog::setName(const QString &name) {
    ui->lineEdit->setText(name);
}

void AskNameDialog::setMaxLength(int maxLength) {
    ui->lineEdit->setMaxLength(maxLength);
}
