#ifndef BATTERYWIDGET_H
#define BATTERYWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <cmath>
#include <QPushButton>

class BatteryWidget : public QPushButton {

    Q_OBJECT

public:
    explicit BatteryWidget(QWidget* parent = nullptr) : QPushButton(parent) {
        setFlat(true);
        setStyleSheet("background: transparent; border: none;");
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setPercentage(int newPercentage) {
        percentage = std::clamp(newPercentage, 0, 100);
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int batW = 45;
        int batH = 22;

        int x = (width() - batW) / 2;
        int y = (height() - batH) / 2 - 10;

        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(x, y, batW, batH, 3, 3);

        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawRect(x + batW + 2, y + batH / 4, 4, batH / 2);

        int fillWidth = (batW - 4) * percentage / 100.0;
        if (percentage > 50) p.setBrush(QColor("#00FF00"));
        else if (percentage > 20) p.setBrush(QColor("#FFA500"));
        else p.setBrush(QColor("#FF0000"));

        if (fillWidth > 0) {
            p.drawRoundedRect(x + 2, y + 2, fillWidth, batH - 4, 1, 1);
        }

        p.setPen(Qt::white);
        QFont font("Arial", 12, QFont::Bold);
        p.setFont(font);
        p.drawText(0, y + batH + 8, width(), 20, Qt::AlignCenter, QString::number(percentage) + "%");
    }

private:
    int percentage = 0;
};

#endif