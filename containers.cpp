#include "containers.h"
#include "constants.h"
#include "hudwidgets.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include "GcsGUI.h"
#include <QCheckBox>
#include <QPushButton>
#include <QFont>
#include <QtMath>
#include <QGraphicsDropShadowEffect>

namespace {

    const char* NEON = "#4fb0ff";
    const char* UNIT = "#7aa7d6";

    QString valueHtml(double v, const QString& unit) {
        return QString("<span style='color:%1;'>%2</span>"
            "<span style='color:%3; font-size:15pt;'>&nbsp;%4</span>")
            .arg(NEON).arg(v, 0, 'f', 2).arg(UNIT).arg(unit.toHtmlEscaped());
    }

    QString barStyle(const QString& color) {
        QColor c(color);
        QColor c0 = c; c0.setAlpha(110);
        return QString(
            "QProgressBar { background: rgba(255,255,255,22); border: none; border-radius: 5px; }"
            "QProgressBar::chunk { border-radius: 5px;"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %1, stop:1 %2); }"
        ).arg(c0.name(QColor::HexArgb), c.name());
    }

    QGraphicsDropShadowEffect* makeGlow(QObject* parent, const QColor& c, int blur) {
        auto* g = new QGraphicsDropShadowEffect(parent);
        QColor col = c; col.setAlpha(170);
        g->setBlurRadius(blur);
        g->setColor(col);
        g->setOffset(0);
        return g;
    }

    double mapValueLocal(double x, double in_min, double in_max, double out_min, double out_max) {
        if (x < in_min) x = in_min;
        if (x > in_max) x = in_max;
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }
}

Containers::Containers(const QString& title, int titleSize, const QString& titleColor,
    double initialValue, bool hasBar, GcsGUI* guiMaster,
    QWidget* parent)
    : QFrame(parent),
    title_text(title),
    title_label_size(titleSize),
    title_label_color(titleColor),
    value(initialValue),
    unit(units_map.value(title)),
    has_bar(hasBar),
    quantity(actual_quantity.value(title)),
    m_guiMaster(guiMaster)
{
    low_bound = bounds_map.value(quantity).first;
    high_bound = bounds_map.value(quantity).second;

    QVector<QString> colors = progress_bar_colors_map.value(quantity);
    if (colors.size() >= 3) {
        low_color = colors[0];
        mid_color = colors[1];
        high_color = colors[2];
    }
    else {
        low_color = THEME_COLOR;
        mid_color = THEME_COLOR;
        high_color = THEME_COLOR;
        qWarning() << "Warning: Missing color definitions for quantity:" << quantity;
    }

    setObjectName("Containers");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "QFrame#Containers {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "              stop:0 rgba(22,32,52,215), stop:1 rgba(9,15,29,225));"
        "  border: 1px solid rgba(79,176,255,70);"
        "  border-radius: 22px;"
        "}");

    design();
}

void Containers::design() {

    QVBoxLayout* frameLayout = new QVBoxLayout(this);
    frameLayout->setContentsMargins(24, 18, 24, 18);
    frameLayout->setSpacing(10);
    frameLayout->setAlignment(Qt::AlignCenter);

    // ---- Τίτλος: μικρός, uppercase, letter-spaced
    QFont titleFont(Hud::family(), 21, QFont::DemiBold);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 3);

    title_label = new QLabel(title_text.toUpper(), this);
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setFont(titleFont);
    title_label->setStyleSheet(QString("background: transparent; border: none; color: %1;")
        .arg(title_label_color == "white" ? QString(UNIT) : title_label_color));
    frameLayout->addWidget(title_label, 0, Qt::AlignHCenter);

    // ---- Μεγάλη τιμή (όχι για gauge κάρτες)
    const bool isGauge = (quantity == "Throttle" || quantity == "Power");

    if (!isGauge) {
        QFont valueFont(Hud::family(), 40, QFont::Bold);

        value_label = new QLabel(valueHtml(value, unit), this);
        value_label->setTextFormat(Qt::RichText);
        value_label->setAlignment(Qt::AlignCenter);
        value_label->setFont(valueFont);
        value_label->setStyleSheet("background: transparent; border: none;");
        value_label->setGraphicsEffect(makeGlow(this, QColor(THEME_COLOR), 22));

        frameLayout->addWidget(value_label, 0, Qt::AlignHCenter);
    }

    // ---- Γραφικό
    if (isGauge) {
        mainGauge = new Gauge(this);
        mainGauge->setValue(value);
        frameLayout->addWidget(mainGauge, 1);
    }
    else if (!has_bar) {
        bigBatteryWidget = new BigBatteryWidget(this);
        frameLayout->addWidget(bigBatteryWidget, 0, Qt::AlignHCenter);
    }

    if (has_bar) {
        progress_bar = new QProgressBar(this);
        progress_bar->setFixedHeight(10);
        progress_bar->setMinimumWidth(240);
        progress_bar->setMaximumWidth(460);
        progress_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        progress_bar->setRange(0, qRound(value_limit.value(quantity)));
        progress_bar->setValue(qRound(value));
        progress_bar->setTextVisible(false);
        progress_bar->setStyleSheet(barStyle(THEME_COLOR));
        progress_bar->setGraphicsEffect(makeGlow(this, QColor(THEME_COLOR), 18));

        frameLayout->addWidget(progress_bar, 0, Qt::AlignHCenter);
    }
}

void Containers::checkBoundsAndSetColor(double currentValue) {

    if (!has_bar || !progress_bar) return;

    QString progressColor = THEME_COLOR;

    if (quantity != "Throttle" && quantity != "RPM") {
        if (currentValue <= low_bound)       progressColor = low_color;
        else if (currentValue < high_bound)  progressColor = mid_color;
        else                                 progressColor = high_color;
    }

    // Restyle μόνο όταν αλλάζει χρώμα (όχι σε κάθε tick του timer)
    if (progress_bar->property("barColor").toString() == progressColor) return;
    progress_bar->setProperty("barColor", progressColor);

    progress_bar->setStyleSheet(barStyle(progressColor));
    if (auto* g = qobject_cast<QGraphicsDropShadowEffect*>(progress_bar->graphicsEffect())) {
        QColor c(progressColor); c.setAlpha(170);
        g->setColor(c);
    }
}

void Containers::updateData(double newValue) {

    value = newValue;

    if (value_label) {
        value_label->setText(valueHtml(value, unit));
    }

    if (quantity == "Throttle" || quantity == "Power") {
        if (mainGauge) mainGauge->setValue(value);
    }
    else if (!has_bar && bigBatteryWidget) {
        bigBatteryWidget->setPercentage(
            mapValueLocal(value, m_guiMaster->min_batt_voltage, m_guiMaster->max_batt_voltage, 0.0, 100.0));
    }

    if (has_bar && progress_bar) {
        const int lim = qRound(value_limit.value(quantity));
        progress_bar->setValue(qBound(0, qRound(value), lim));
        checkBoundsAndSetColor(value);
    }
}