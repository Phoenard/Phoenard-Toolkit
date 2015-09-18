#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include "controls/sdbrowserwidget.h"
#include "controls/menubutton.h"
#include "controls/imageviewer.h"
#include <QListView>
#include <QPushButton>
#include <QFileDialog>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void closeEvent(QCloseEvent *);
    void openSerial();
    void setSelectedTab(int index, bool forceUpdate = false);
    int selectedTab();
    void setControlMode(int mode);
    void img_updateFormat();
    void img_load(QString fileName, ImageFormat format);
    void showSaveDialog(QWidget *at, ImageViewer *editorDialog);

private slots:
    /* Custom slots */
    void serial_statusChanged(QString status);
    void serial_closed();

    /* Qt-generated slots */
    void on_port_toggleBtn_clicked();

    void on_port_nameBox_activated(int index);

    void on_serialButton_clicked();

    void on_sketchesButton_clicked();

    void on_microSDButton_clicked();

    void on_chipControlButton_clicked();

    void on_sd_refreshButton_clicked();

    void on_sd_deleteButton_clicked();

    void on_microSDWidget_itemSelectionChanged();

    void on_sd_saveToButton_clicked();

    void on_sd_importFromButton_clicked();

    void on_sd_newButton_clicked();

    void on_imageEditorButton_clicked();

    void on_img_formatButton_clicked();

    void on_img_loadButton_clicked();

    void on_img_saveButton_clicked();

    void on_serial_shareMode_toggled(bool checked);

    void on_sketches_runBtn_clicked();

    void on_sketches_iconBtn_clicked();

    void on_sketches_addnewBtn_clicked();

    void on_sketches_deleteBtn_clicked();

    void on_sketches_renameBtn_clicked();

    void on_control_pinsBtn_clicked();

    void on_control_registersBtn_clicked();

    void on_control_spiBtn_clicked();

    void on_control_firmwareBtn_clicked();

    void on_serial_deviceMode_clicked();

private:
    Ui::MainWindow *ui;
    stk500Serial *serial;
    MenuButton **allButtons;
    MenuButton **controlButtons;
    int allButtons_len;
    int controlButtons_len;
    QIcon fmt_icons[8];
};

/* Global functions used during startup */
void loadFont(const QString& fontPath);

#endif // MAINWINDOW_H
