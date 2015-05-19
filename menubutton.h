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
    const bool isTab() { return _isTab; }
    void setIsTab(bool isTab);

    void MenuButton::enterEvent(QEvent *event);
    void MenuButton::leaveEvent(QEvent *event);

signals:

public slots:

private:
    bool _isTab;
    bool _isHover;
};

#endif // MENUBUTTON_H
