#ifndef MENUBUTTON_H
#define MENUBUTTON_H

#include <QToolButton>
#include <QPaintEvent>

class MenuButton : public QToolButton
{
    Q_OBJECT
public:
    explicit MenuButton(QWidget *parent = 0);
    void paintEvent(QPaintEvent *e);
    bool isTab() { return _isTab; }
    void setIsTab(bool isTab);

    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);

signals:

public slots:

private:
    bool _isTab;
    bool _isHover;
};

#endif // MENUBUTTON_H
