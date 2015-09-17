#ifndef PHNBUTTON_H
#define PHNBUTTON_H

#include <QPushButton>

class PHNButton : public QPushButton
{
    Q_OBJECT
public:
    explicit PHNButton(QWidget *parent = 0);
    void paintEvent(QPaintEvent *e);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);

    static void drawBase(QWidget *widget, bool isHover, bool isDown);
    static QColor getGradientFactor(const QColor &ca, const QColor &cb);
    static QColor getColorFact(const QColor &c, qreal factor);
    static QColor getColorFact(const QColor &c, qreal fr, qreal fg, qreal fb);

signals:

public slots:

private:
    bool isMouseOver;

};

#endif // PHNBUTTON_H
