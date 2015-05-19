#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include "stk500/stk500Session.h"
#include <QSerialPortInfo>
#include <QTimer>
#include <QMessageBox>
#include <QDirModel>
#include <QPalette>
#include <QFileDialog>
#include <QMenu>
#include <QPoint>
#include <QScrollBar>
#include "formatselectdialog.h"

stk500Session serial;
MenuButton **allButtons;
const int allButtons_len = 5;
QIcon fmt_icons[8];
QTimer *serial_updateTimer;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set up image format icons
    fmt_icons[0] = QIcon(":/icons/fmt_lcd1.png");
    fmt_icons[1] = QIcon(":/icons/fmt_lcd2.png");
    fmt_icons[2] = QIcon(":/icons/fmt_lcd4.png");
    fmt_icons[3] = QIcon(":/icons/fmt_lcd8.png");
    fmt_icons[4] = QIcon(":/icons/fmt_lcd16.png");
    fmt_icons[5] = QIcon(":/icons/fmt_bmp8.png");
    fmt_icons[6] = QIcon(":/icons/fmt_bmp24.png");

    // Set up buttons
    allButtons = new MenuButton*[allButtons_len];
    allButtons[0] = ui->serialButton;
    allButtons[1] = ui->sketchesButton;
    allButtons[2] = ui->microSDButton;
    allButtons[3] = ui->memoriesButton;
    allButtons[4] = ui->imageEditorButton;

    for (QSerialPortInfo info : QSerialPortInfo::availablePorts()) {
        ui->comboBox->addItem(info.portName());
    }
    ui->microSDWidget->setAcceptDrops(true);
    serial.open(ui->comboBox->currentText());
    ui->microSDWidget->setSession(&serial);

    QString settingStyle = " QMainWindow {\
            background-color: #980000;\
    }\
     QStackedWidget#mainTabs {\
        background-color: #FFFFFF;\
    }";

    setStyleSheet(settingStyle);



    // Set main menu buttons to TABs
    for (int i = 0; i < allButtons_len; i++) {
        allButtons[i]->setIsTab(true);
    }
    setSelectedTab(0, true);

    // Load an image for debugging
    img_load("C:/Users/QT/Desktop/test24.bmp", LCD4);
    ui->img_editor->saveImageTo("C:/Users/QT/Desktop/tmp.lcd");

    serial_updateTimer = new QTimer(this);
    connect(serial_updateTimer, SIGNAL(timeout()), this, SLOT(on_serial_timer_ticked()));
    serial_updateTimer->start(50);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    if (!serial.abort()) {
        QMessageBox msgBox;
        msgBox.critical(this, "Closing", "Failed to close one or more open ports");
        event->ignore();
    } else {
        event->accept();
    }
    refreshSerial();
}

void MainWindow::refreshSerial() {
    if (serial.isOpen()) {
        this->ui->serial_toggleButton->setText("Close");
    } else {
        this->ui->serial_toggleButton->setText("Open");
    }
}

void MainWindow::on_serial_toggleButton_clicked()
{
    if (serial.isOpen()) {
        serial.close();
    } else if (ui->comboBox->currentIndex() != -1) {
        serial.open(ui->comboBox->currentText());
    }
    refreshSerial();
}

void MsgBox(QString & text) {
    QMessageBox msgBox;
    msgBox.setText(text);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

void MainWindow::on_comboBox_currentIndexChanged(int index) {
    if (index == -1) {
        serial.close();
    } else {
        serial.open(ui->comboBox->itemText(index));
    }
    refreshSerial();
}

void MainWindow::setSelectedTab(int index, bool forceUpdate) {
    bool doEvents = forceUpdate || (index != ui->mainTabs->currentIndex());
    ui->optionTabs->setCurrentIndex(index);
    ui->mainTabs->setCurrentIndex(index);
    MenuButton *selectedBtn = NULL;
    if (index >= 0 && index < allButtons_len) {
        selectedBtn = allButtons[index];
    }

    if (index == 2) {
        if (doEvents) {
            ui->microSDWidget->refreshFiles();
            on_microSDWidget_itemSelectionChanged();
        }
    }


    for (int i = 0; i < allButtons_len; i++) {
        allButtons[i]->setChecked(allButtons[i] == selectedBtn);
    }
}

void MainWindow::on_serialButton_clicked() {
    setSelectedTab(0);
}

void MainWindow::on_sketchesButton_clicked() {
    setSelectedTab(1);
}

void MainWindow::on_microSDButton_clicked() {
    setSelectedTab(2);
}

void MainWindow::on_memoriesButton_clicked() {
    setSelectedTab(3);
}

void MainWindow::on_imageEditorButton_clicked() {
    setSelectedTab(4);
}

void MainWindow::on_sd_refreshButton_clicked()
{
    ui->microSDWidget->refreshFiles();
}

void MainWindow::on_sd_deleteButton_clicked()
{
    ui->microSDWidget->deleteFiles();
}

void MainWindow::on_microSDWidget_itemSelectionChanged()
{
    bool hasSel = ui->microSDWidget->hasSelection();
    ui->sd_deleteButton->setEnabled(hasSel);
    ui->sd_saveToButton->setEnabled(hasSel);
    bool hasFolderSel = (ui->microSDWidget->getSelectedFolder() != NULL);
    ui->sd_newButton->setEnabled(hasFolderSel);
    ui->sd_importFromButton->setEnabled(hasFolderSel);
}

void MainWindow::on_sd_saveToButton_clicked()
{
    ui->microSDWidget->saveFilesTo();
}

void MainWindow::on_sd_importFromButton_clicked()
{
    ui->microSDWidget->importFiles();
}

QAction *showMenu(QWidget *widget, QMenu &menu) {
    QPoint menuPos = widget->mapToGlobal(QPoint(0,widget->height()));
    //menu.setStyleSheet("background-color: #980000;");
    return menu.exec(menuPos);
}

void MainWindow::on_sd_newButton_clicked()
{
    QIcon icon_folder(":/icons/newfolder.png");
    QIcon icon_file(":/icons/newfile.png");
    QMenu myMenu;
    QAction *folderAction = myMenu.addAction(icon_folder, "Folder");
    QAction *fileAction = myMenu.addAction(icon_file, "File");
    QAction *imageAction = myMenu.addAction("Image");
    QAction *clicked = showMenu(ui->sd_newButton, myMenu);
    if (!clicked) {
        return;
    }
    if (clicked == folderAction) {
        ui->microSDWidget->createNew("New Folder", true);
    } else if (clicked == fileAction) {
        ui->microSDWidget->createNew("New File", false);
    } else if (clicked == imageAction) {
        qDebug() << "TODO...";
    }
}

void MainWindow::img_updateFormat() {
    ImageFormat format = ui->img_editor->outputImageFormat();
    ui->img_formatButton->setIcon(fmt_icons[(int) format]);
    ui->img_colorSelect->setColorMode(format);

    if (!ui->img_editor->outputImageTrueColor()) {
        for (int i = 0; i < ui->img_editor->getColorCount(); i++) {
            ui->img_colorSelect->setColor(i, ui->img_editor->getColor(i));
        }
    }
}

void MainWindow::on_img_formatButton_clicked()
{
    QMenu myMenu;
    QAction *actions[7];
    for (int i = 0; i < 7; i++) {
        actions[i] = myMenu.addAction(fmt_icons[i], getFormatName((ImageFormat) i));
    }

    QAction *clicked = showMenu(ui->img_formatButton, myMenu);
    for (int i = 0; i < 7; i++) {
        if (actions[i] == clicked) {
            ui->img_editor->setFormat((ImageFormat) i);
        }
    }
    img_updateFormat();
}

void MainWindow::img_load(QString fileName, ImageFormat format)
{
    ui->img_editor->loadImage(fileName);
    ui->img_editor->setFormat(format);
    img_updateFormat();
}

void MainWindow::on_img_loadButton_clicked()
{
    //TODO: Make a set of buttons when clicked, expand to show the browser for the type specified
    //For example, pressing the micro-sd button slider opens it and shows an SD browser widget

    QIcon icon_file(":/icons/newfile.png");
    QIcon icon_sd(":/icons/microsd.png");
    QIcon icon_code(":/icons/code.png");
    QMenu menu;
    QAction *fileOpt = menu.addAction(icon_file, "Load from file");
    QAction *sdOpt = menu.addAction(icon_sd, "Load from Micro-SD");
    QAction *codeOpt = menu.addAction(icon_code, "Load from code");
    QAction *result = showMenu(ui->img_loadButton, menu);

    QByteArray data;
    if (result == fileOpt) {
        QFileDialog dialog(this);
        dialog.setWindowTitle("Select the image file to load");
        if (!dialog.exec()) {
            return;
        }
        QString filePath = dialog.selectedFiles().at(0);
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            data = file.readAll();
            file.close();
        } else {
            //TODO: MESSAGE
            return;
        }
    } else if (result == sdOpt) {
        //TODO: Show dialog for browsing on the micro-SD
        return;
    } else if (result == codeOpt) {
        //TODO: Show dialog for copy-pasting data
        return;
    } else {
        // No selection
        return;
    }

    // Show format selection dialog
    FormatSelectDialog dialog;
    dialog.setData(ui->img_editor, data);
    dialog.exec();

    // All set and done; update the format button icon
    img_updateFormat();
}

void MainWindow::on_img_saveButton_clicked()
{
    QFileDialog dialog(this);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle("Select the destination location to save to");
    if (!dialog.exec()) {
        return;
    }
    QString filePath = dialog.selectedFiles().at(0);
    ui->img_editor->saveImageTo(filePath);
}

void MainWindow::on_serial_timer_ticked()
{
    // Read in data like woo!
    char buff[201];
    int len = serial.read(buff, 200);
    if (len) {
        buff[len] = 0;
        QString myString(buff);

        if (ui->serial_autoScroll->isChecked()) {
            ui->serial_outputText->moveCursor (QTextCursor::End);
            ui->serial_outputText->insertPlainText(myString);
            ui->serial_outputText->moveCursor (QTextCursor::End);
        } else {
            int old_scroll = ui->serial_outputText->verticalScrollBar()->value();
            QTextCursor old_cursor = ui->serial_outputText->textCursor();
            ui->serial_outputText->moveCursor (QTextCursor::End);
            ui->serial_outputText->insertPlainText(myString);
            ui->serial_outputText->setTextCursor(old_cursor);
            ui->serial_outputText->verticalScrollBar()->setValue(old_scroll);
        }
    }
}
