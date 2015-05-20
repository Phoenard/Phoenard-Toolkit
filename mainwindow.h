#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include "sdbrowserwidget.h"
#include "menubutton.h"
#include "imageeditor.h"
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
    void closeEvent(QCloseEvent *event);
    void openSerial();
    void setSelectedTab(int index, bool forceUpdate = false);
    int selectedTab();
    void img_updateFormat();
    void img_load(QString fileName, ImageFormat format);

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

    void on_memoriesButton_clicked();

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

private:
    Ui::MainWindow *ui;
    stk500Serial *serial;
    MenuButton **allButtons;
    int allButtons_len;
    QIcon fmt_icons[8];
};

#endif // MAINWINDOW_H
