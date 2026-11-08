#pragma once
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFontDatabase>
#include <QtMath>

namespace Hud {

    // Φορτώνει την Orbitron ΜΙΑ φορά. Αν λείπει το αρχείο -> fallback, όχι crash.
    inline const QString& family() {
        static const QString fam = []() {
            int id = QFontDatabase::addApplicationFont("./assets/Orbitron/Orbitron-VariableFont_wght.ttf");
            if (id >= 0) {
                const QStringList l = QFontDatabase::applicationFontFamilies(id);
                if (!l.isEmpty()) return l.first();
            }
            return QString("Segoe UI");
            }();
        return fam;
    }

    inline QColor levelColor(double pct) {
        if (pct < 20.0) return QColor("#f87171");
        if (pct < 50.0) return QColor("#fbbf24");
        return QColor("#34d399");
    }
}

// ---------------------------------------------------------------- Gauge
class Gauge : public QWidget {
    Q_OBJECT
public:
    explicit Gauge(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(200, 150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setValue(double v) {
        v = qBound(0.0, v, 100.0);
        if (qFuzzyCompare(m_value + 1.0, v + 1.0)) return;
        m_value = v;
        update();
    }
    void setUnit(const QString& u) { m_unit = u; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const double side = qMin(width(), height());
        p.translate(width() / 2.0, height() / 2.0);
        p.scale(side / 220.0, side / 220.0);       // λογικές συντεταγμένες -110..110

        const QColor accent = Hud::levelColor(100.0 - m_value < 0 ? 100 : 100.0 - m_value);
        // Για power/throttle: χαμηλά = πράσινο, ψηλά = κόκκινο
        QColor valCol = m_value < 60 ? QColor("#34d399")
            : m_value < 85 ? QColor("#fbbf24")
            : QColor("#f87171");
        Q_UNUSED(accent);

        // Ticks
        const int N = 21;
        for (int i = 0; i < N; ++i) {
            const double a = qDegreesToRadians(225.0 - i * (270.0 / (N - 1)));
            const double r1 = 100.0, r2 = (i % 5 == 0) ? 90.0 : 94.0;
            const bool on = (i * 100.0 / (N - 1)) <= m_value;
            QColor tc = on ? valCol : QColor(255, 255, 255, 45);
            p.setPen(QPen(tc, (i % 5 == 0) ? 2.2 : 1.4, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(qCos(a) * r1, -qSin(a) * r1), QPointF(qCos(a) * r2, -qSin(a) * r2));
        }

        const QRectF arc(-78, -78, 156, 156);

        // Track
        p.setPen(QPen(QColor(255, 255, 255, 26), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arc, 225 * 16, -270 * 16);

        // Value arc + glow
        if (m_value > 0.4) {
            const int span = int(-270.0 * 16.0 * m_value / 100.0);
            QColor glow = valCol; glow.setAlpha(55);
            p.setPen(QPen(glow, 20, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(arc, 225 * 16, span);
            p.setPen(QPen(valCol, 10, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(arc, 225 * 16, span);
        }

        // Number
        QFont f(Hud::family());
        f.setBold(true);
        f.setPixelSize(44);
        p.setFont(f);
        p.setPen(QColor("#4fb0ff"));
        p.drawText(QRectF(-70, -32, 140, 56), Qt::AlignCenter, QString::number(qRound(m_value)));

        // Unit
        QFont u(Hud::family());
        u.setPixelSize(13);
        u.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        p.setFont(u);
        p.setPen(QColor("#7aa7d6"));
        p.drawText(QRectF(-60, 24, 120, 20), Qt::AlignCenter, m_unit);
    }

private:
    double  m_value = 0.0;
    QString m_unit = "%";
};

// ------------------------------------------------------ BigBatteryWidget
class BigBatteryWidget : public QWidget {
    Q_OBJECT
public:
    explicit BigBatteryWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(260, 64);
        setMaximumWidth(460);
        setFixedHeight(64);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setPercentage(double pct) {
        pct = qBound(0.0, pct, 100.0);
        if (qFuzzyCompare(m_pct + 1.0, pct + 1.0)) return;
        m_pct = pct;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF body(2, 2, width() - 16, height() - 4);
        const QRectF pole(body.right() + 2, height() * 0.32, 8, height() * 0.36);
        const double rad = 14;

        // Body
        p.setPen(QPen(QColor(255, 255, 255, 70), 1.5));
        p.setBrush(QColor(255, 255, 255, 14));
        p.drawRoundedRect(body, rad, rad);

        // Pole
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 70));
        p.drawRoundedRect(pole, 3, 3);

        // Fill (clip στο σχήμα του body)
        const QColor c = Hud::levelColor(m_pct);
        QPainterPath clip;
        clip.addRoundedRect(body.adjusted(4, 4, -4, -4), rad - 4, rad - 4);
        p.save();
        p.setClipPath(clip);
        const QRectF inner = body.adjusted(4, 4, -4, -4);
        QRectF fill = inner;
        fill.setWidth(inner.width() * m_pct / 100.0);
        QLinearGradient g(fill.topLeft(), fill.topRight());
        QColor c0 = c; c0.setAlpha(120);
        g.setColorAt(0, c0);
        g.setColorAt(1, c);
        p.setBrush(g);
        p.drawRect(fill);

        // Segment separators κάθε 20%
        p.setPen(QPen(QColor(0, 0, 0, 80), 1.5));
        for (int i = 1; i < 5; ++i) {
            const double x = inner.left() + inner.width() * i / 5.0;
            p.drawLine(QPointF(x, inner.top()), QPointF(x, inner.bottom()));
        }
        p.restore();

        // Text
        QFont f(Hud::family());
        f.setBold(true);
        f.setPixelSize(int(height() * 0.40));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 235));
        p.drawText(body, Qt::AlignCenter, QString("%1%").arg(qRound(m_pct)));
    }

private:
    double m_pct = 0.0;
};