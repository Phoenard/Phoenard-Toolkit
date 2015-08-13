#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include "stk500/stk500serial.h"
#include <QSerialPortInfo>
#include <QTimer>
#include <QMessageBox>
#include <QDirModel>
#include <QPalette>
#include <QFileDialog>
#include <QMenu>
#include <QPoint>
#include <QScrollBar>
#include "dialogs/formatselectdialog.h"
#include "dialogs/codeselectdialog.h"
#include "dialogs/iconeditdialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialize serial
    serial = new stk500Serial(this);
    ui->microSDWidget->setSerial(serial);
    ui->serialmonitor->setSerial(serial);
    ui->sketchesWidget->setSerial(serial);

    // Connect serial signal events
    connect(serial, SIGNAL(statusChanged(QString)),
            this,   SLOT(serial_statusChanged(QString)),
            Qt::QueuedConnection);

    connect(serial, SIGNAL(closed()),
            this,   SLOT(serial_closed()),
            Qt::QueuedConnection);

    // Connect sketch list item double-click to sketch run
    connect(ui->sketchesWidget, SIGNAL(sketchDoubleClicked()),
            this, SLOT(on_sketches_runBtn_clicked()),
            Qt::QueuedConnection);

    // Set up image format icons
    fmt_icons[0] = QIcon(":/icons/fmt_lcd1.png");
    fmt_icons[1] = QIcon(":/icons/fmt_lcd2.png");
    fmt_icons[2] = QIcon(":/icons/fmt_lcd4.png");
    fmt_icons[3] = QIcon(":/icons/fmt_lcd8.png");
    fmt_icons[4] = QIcon(":/icons/fmt_lcd16.png");
    fmt_icons[5] = QIcon(":/icons/fmt_bmp8.png");
    fmt_icons[6] = QIcon(":/icons/fmt_bmp24.png");

    // Set up buttons
    allButtons_len = 5;
    allButtons = new MenuButton*[allButtons_len];
    allButtons[0] = ui->serialButton;
    allButtons[1] = ui->sketchesButton;
    allButtons[2] = ui->microSDButton;
    allButtons[3] = ui->memoriesButton;
    allButtons[4] = ui->imageEditorButton;

    for (QSerialPortInfo info : QSerialPortInfo::availablePorts()) {
        ui->port_nameBox->addItem(info.portName());
    }
    ui->microSDWidget->setAcceptDrops(true);

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
    setSelectedTab(1, true);

    this->openSerial();

    // Load an image for debugging
    //img_load("C:/Users/QT/Desktop/test24.bmp", LCD4);
    //ui->img_editor->saveImageTo("C:/Users/QT/Desktop/tmp.lcd");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent (QCloseEvent *)
{
    serial->close();
}

void MainWindow::openSerial()
{
    int index = ui->port_nameBox->currentIndex();
    if (index == -1) {
        serial->close();
    } else {
        serial->open(ui->port_nameBox->itemText(index));
        ui->port_toggleBtn->setText("Close");
    }
    /* Notify current tab */
    setSelectedTab(this->selectedTab(), true);
}

void MainWindow::on_port_nameBox_activated(int)
{
    openSerial();
}

void MainWindow::on_port_toggleBtn_clicked()
{
    if (serial->isOpen()) {
        serial->close();
    } else {
        openSerial();
    }
}

void MainWindow::serial_closed()
{
    ui->port_toggleBtn->setText("Open");
}

void MainWindow::serial_statusChanged(QString status)
{
    ui->port_statusLbl->setText(status);
}

void MsgBox(QString & text) {
    QMessageBox msgBox;
    msgBox.setText(text);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

int MainWindow::selectedTab() {
    return ui->mainTabs->currentIndex();
}

void MainWindow::setSelectedTab(int index, bool forceUpdate) {
    int prevIndex = selectedTab();
    bool doEvents = forceUpdate || (index != prevIndex);
    ui->optionTabs->setCurrentIndex(index);
    ui->mainTabs->setCurrentIndex(index);
    MenuButton *selectedBtn = NULL;
    if (index >= 0 && index < allButtons_len) {
        selectedBtn = allButtons[index];
    }

    /* Handle events fired when a tab is closed */
    if (prevIndex != index) {
        /* Close serial when closing serial tab */
        if (prevIndex == 0) {
            serial->closeSerial();
        }

        /* Stop loading icons when closing sketches tab */
        if (prevIndex == 1) {
            ui->sketchesWidget->stopLoadingIcons();
        }
    }

    if (doEvents) {
        /* First tab uses serial mode, all others rely on stk500 */
        if (index == 0) {
            ui->serialmonitor->openSerial();
        }

        /* Load sketches when opening sketches tab */
        if (index == 1) {
            ui->sketchesWidget->refreshSketches();
        }

        /* Refresh when opening SD tab */
        if (index == 2) {
            ui->microSDWidget->refreshFiles();
            on_microSDWidget_itemSelectionChanged();
        }

        /* Refresh image format when opening Images tab */
        if (index == 4) {
            img_updateFormat();
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
    PHNImage &image = ui->img_editor->image();
    bool hasImage = !image.isNull();
    ui->img_formatButton->setEnabled(hasImage);
    ui->img_saveButton->setEnabled(hasImage);
    if (hasImage) {
        ImageFormat format = image.outputImageFormat();
        ui->img_formatButton->setIcon(fmt_icons[(int) format]);
        ui->img_colorSelect->setColorMode(format);

        if (!image.isFullColor()) {
            for (int i = 0; i < image.getColorCount(); i++) {
                ui->img_colorSelect->setColor(i, image.getColor(i));
            }
        }
    }
}

void MainWindow::on_img_formatButton_clicked()
{
    PHNImage &image = ui->img_editor->image();
    QMenu myMenu;
    QAction *actions[7];
    for (int i = 0; i < 7; i++) {
        actions[i] = myMenu.addAction(fmt_icons[i], getFormatName((ImageFormat) i));
    }
    QString useHeaderTitle = "Use image header";
    QAction *useHeaderItem;
    if (image.hasHeader()) {
        QIcon useHeaderIcon(":/icons/checkbox_checked.png");
        useHeaderItem = myMenu.addAction(useHeaderIcon, useHeaderTitle);
    } else {
        QIcon useHeaderIcon(":/icons/checkbox_unchecked.png");
        useHeaderItem = myMenu.addAction(useHeaderIcon, useHeaderTitle);
    }

    QAction *clicked = showMenu(ui->img_formatButton, myMenu);
    if (clicked == useHeaderItem) {
        image.setHeader(!image.hasHeader());
    } else {
        for (int i = 0; i < 7; i++) {
            if (actions[i] == clicked) {
                image.setFormat((ImageFormat) i);
            }
        }
        img_updateFormat();
    }
}

void MainWindow::img_load(QString fileName, ImageFormat format)
{
    PHNImage &image = ui->img_editor->image();
    image.loadFile(fileName);
    image.setFormat(format);
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
        /* Browse a file on the local filesystem */
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
        /* Allow the user to paste code to load in */
        CodeSelectDialog dialog(this);
        dialog.setMode(CodeSelectDialog::OPEN);
        if (dialog.exec()) {
            data = dialog.getData();
        } else {
            return;
        }
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

void MainWindow::showSaveDialog(QWidget *at, ImageViewer *editorDialog) {
    QIcon icon_file(":/icons/newfile.png");
    QIcon icon_sd(":/icons/microsd.png");
    QIcon icon_code(":/icons/code.png");
    QMenu menu;
    QAction *fileOpt = menu.addAction(icon_file, "Save to file");
    QAction *sdOpt = menu.addAction(icon_sd, "Save to Micro-SD");
    QAction *codeOpt = menu.addAction(icon_code, "Save as code");
    QAction *result = showMenu(at, menu);

    if (result == fileOpt) {
        /* Browse a file on the local filesystem */
        QFileDialog dialog(this);
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setWindowTitle("Select the destination location to save to");
        if (!dialog.exec()) {
            return;
        }
        QString filePath = dialog.selectedFiles().at(0);
        editorDialog->image().saveImageTo(filePath);
    } else if (result == sdOpt) {
        //TODO: Show dialog for browsing on the micro-SD
        return;
    } else if (result == codeOpt) {
        QByteArray data = editorDialog->image().saveImage();

        /* Allow the user to copy code  */
        CodeSelectDialog dialog(this);
        dialog.setMode(CodeSelectDialog::SAVE);
        dialog.setData(data);
        dialog.exec();
    } else {
        // No selection
        return;
    }
}

void MainWindow::on_img_saveButton_clicked()
{
    showSaveDialog(ui->img_loadButton, ui->img_editor);
}

void MainWindow::on_serial_shareMode_toggled(bool checked)
{
    ui->serialmonitor->setScreenShare(checked);
}

void MainWindow::on_serial_saveImage_clicked()
{
    showSaveDialog(ui->serial_saveImage, ui->serialmonitor->getImageEditor());
}

void MainWindow::on_sketches_runBtn_clicked()
{
    if (!ui->sketchesWidget->hasSelectedSketch()) {
        return;
    }
    ui->sketchesWidget->stopLoadingIcons();
    QString name = ui->sketchesWidget->getSelectedName();
    serial->execute(stk500LaunchSketch(name));
    setSelectedTab(0);
}

void MainWindow::on_sketches_iconBtn_clicked()
{
    if (!ui->sketchesWidget->hasSelectedSketch()) {
        return;
    }
    SketchInfo info = ui->sketchesWidget->getSelectedSketch();
    IconEditDialog dialog(this);
    dialog.setModal(true);
    dialog.setWindowTitle(QString("Editing icon of ") + QString(info.name));
    dialog.loadIcon(info.iconData);
    if (dialog.exec() == QDialog::Accepted) {
        QString fileName = QString(info.name) + ".SKI";
        QString tmpFile = stk500::getTempFile(fileName);

        // Save the icon
        dialog.saveIcon(info.iconData);
        info.setIcon(info.iconData);

        // Write the new icon data to a temporary file
        QFile file(tmpFile);
        file.open(QIODevice::WriteOnly);
        file.write(info.iconData, sizeof(info.iconData));
        file.close();

        // Save this file to SD
        stk500ImportFiles task(tmpFile, fileName);
        serial->execute(task);

        // Update icon in sketch list if successful
        if (!task.hasError() && !task.isCancelled()) {
            ui->sketchesWidget->setSelectedSketch(info);
        }

        // Delete the temporary file again
        file.remove();
    }
}
