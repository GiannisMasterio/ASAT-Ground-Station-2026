#ifndef GAUGE_H
#define GAUGE_H

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QConicalGradient>
#include <cmath>

class Gauge : public QWidget {
    Q_OBJECT

public:
    explicit Gauge(QWidget* parent = nullptr) : QWidget(parent), value(-1.0) {
        setMinimumSize(150, 150);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }

    void setValue(double val) {
        double newVal = qBound(0.0, val, 100.0);

        // Βελτιστοποίηση: Αν η αλλαγή είναι απειροελάχιστη, αγνόησέ την
        if (std::abs(value - newVal) < 0.1) {
            return;
        }

        value = newVal;
        update(); // Καλεί το paintEvent
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        createCache(); // Φτιάχνει ξανά το στατικό background αν αλλάξει το μέγεθος
    }

    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // 1. Επικόλληση του σταθερού background (Ολόκληρο το χρωματιστό τόξο)
        painter.drawPixmap(0, 0, backgroundCache);

        // Μεταφορά συντεταγμένων στο κέντρο για τη βελόνα και το κείμενο
        int side = qMin(width(), height());
        painter.translate(width() / 2.0, height() / 2.0);
        painter.scale(side / 200.0, side / 200.0);

        // Υπολογισμός γωνίας για τη βελόνα (0-100% -> 0 έως 240 μοίρες)
        double spanAngle = (value / 100.0) * 240.0;

        // 2. Σχεδίαση ΜΟΝΟ της Βελόνας (Πολύ πιο παχιά και ευδιάκριτη)
        painter.save();
        painter.rotate(150 + spanAngle);

        // Σώμα βελόνας (Αυξήθηκε σε 4.5 για να είναι ξεκάθαρη πάνω από το gradient)
        painter.setPen(QPen(Qt::white, 4.5, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(0, 0, 65, 0);

        // Κέντρο βελόνας / Pivot (Μεγάλωσε στα 14px για σωστή αναλογία)
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawEllipse(-7, -7, 14, 14);
        painter.restore();

        // 3. Κείμενο Τιμής (Μεγάλο, Bold, Κεντραρισμένο κάτω από το κέντρο)
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(18); // Από 11 πήγε 18 για να "φωνάζει" η τιμή
        font.setBold(true);
        painter.setFont(font);

        // Τοποθετείται στο κέντρο-κάτω (x: -50 έως 50, y: 35) για να μη μπερδεύεται με το τόξο
        QRect textRect(-50, 35, 100, 30);
        painter.drawText(textRect, Qt::AlignCenter, QString::number(value, 'f', 1));
    }

private:
    double value;
    QPixmap backgroundCache;

    // Δημιουργεί την εικόνα του στατικού background (Γκρι τόξο + Μόνιμα όλο το Gradient)
    void createCache() {
        backgroundCache = QPixmap(size());
        backgroundCache.fill(Qt::transparent);

        QPainter cachePainter(&backgroundCache);
        cachePainter.setRenderHint(QPainter::Antialiasing);

        int side = qMin(width(), height());
        cachePainter.translate(width() / 2.0, height() / 2.0);
        cachePainter.scale(side / 200.0, side / 200.0);

        // Α. Σχεδιάζουμε το γκρι background (για το εφέ σκιάς/βάθους)
        cachePainter.setPen(QPen(QColor(40, 40, 40, 100), 12, Qt::SolidLine, Qt::RoundCap));
        cachePainter.drawArc(-80, -80, 160, 160, 210 * 16, -240 * 16);

        // Β. Σχεδιάζουμε ΠΛΗΡΩΣ το Χρωματιστό Gradient Arc από πάνω (Μόνιμα ανοιχτό)
        QConicalGradient gradient(0, 0, -30);
        gradient.setColorAt(0.0, QColor(255, 30, 30));     // Κόκκινο (τέλος)
        gradient.setColorAt(0.333, QColor(255, 220, 0));   // Κίτρινο (μέση)
        gradient.setColorAt(0.666, QColor(0, 255, 50));    // Πράσινο (αρχή)

        // Πάχος τόξου στα 12px για ελαφρώς πιο γεμάτη αίσθηση
        cachePainter.setPen(QPen(QBrush(gradient), 12, Qt::SolidLine, Qt::RoundCap));
        cachePainter.drawArc(-80, -80, 160, 160, 210 * 16, -240 * 16);
    }
};

#endif // GAUGE_H