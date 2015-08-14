#ifndef SKETCHLISTWIDGET_H
#define SKETCHLISTWIDGET_H

#include <QWidget>
#include "mainmenutab.h"
#include <QListWidgetItem>

namespace Ui {
class SketchListWidget;
}

class SketchListWidget : public QWidget, public MainMenuTab
{
    Q_OBJECT

public:
    explicit SketchListWidget(QWidget *parent = 0);
    ~SketchListWidget();
    void refreshSketches();
    bool hasSelectedSketch();
    QString getSelectedName();
    SketchInfo getSelectedSketch();
    void setSelectedSketch(const SketchInfo &sketch);
    void startLoadingIcons();
    void stopLoadingIcons();
    QList<QString> getSketchNames();
    void updateSketch(SketchInfo sketch);
    void deleteSketch(const SketchInfo &sketch);

signals:
    void sketchDoubleClicked();

private slots:
    void serialTaskFinished(stk500Task *task);
    void on_list_itemDoubleClicked(QListWidgetItem *item);

private:
    void setItemSketch(QListWidgetItem *item, const SketchInfo &sketch);
    SketchInfo getItemSketch(const QListWidgetItem *item);

    Ui::SketchListWidget *ui;
    stk500LoadIcon *lastTask;
    QIcon loadIcon;
    QIcon defaultIcon;
};

#endif // SKETCHLISTWIDGET_H
