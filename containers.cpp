#include "containers.h"
#include "constants.h"
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
#include <QFontDatabase>

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

    design();

    setStyleSheet(QString("QFrame { background-color: rgba(30, 34, 43, 200); "
        "border-radius: 20px; "
        "border: 1px solid %1; }").arg(BORDER_COLOR));
}

void Containers::design() {

    int OrbitronfontId = QFontDatabase::addApplicationFont("./assets/Orbitron/Orbitron-VariableFont_wght.ttf");
    QString OrbitronfontFamily = QFontDatabase::applicationFontFamilies(OrbitronfontId).at(0);
    QFont neonFont(OrbitronfontFamily, 54, QFont::Bold);
    QVBoxLayout* frameLayout = new QVBoxLayout(this);
    frameLayout->setAlignment(Qt::AlignCenter);

    title_label = new QLabel(title_text, this);
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setFont(neonFont);
    title_label->setStyleSheet(QString("color: %1; border: none;").arg(title_label_color));

    frameLayout->addWidget(title_label, 0, Qt::AlignCenter);

    if (quantity != "Throttle" && quantity != "Power") {

        QString initialValueText;
        initialValueText = QString("%1 %2").arg(value, 0, 'f', 2).arg(unit);

        value_label = new QLabel(initialValueText, this);
        value_label->setAlignment(Qt::AlignCenter);
        value_label->setFont(neonFont);
        value_label->setStyleSheet(QString("background: transparent; border: none; color: %1;").arg(THEME_COLOR));

        // Εφαρμογή του Glow
        QGraphicsDropShadowEffect* valueGlow = new QGraphicsDropShadowEffect(this);
        valueGlow->setBlurRadius(15); // Μικρότερο radius για πιο "κοφτερό" glow στα γράμματα
        valueGlow->setColor(QColor(THEME_COLOR));
        valueGlow->setOffset(0);
        value_label->setGraphicsEffect(valueGlow);

        frameLayout->addWidget(value_label, 0, Qt::AlignCenter);
    }

    if (quantity == "Throttle" || quantity == "Power") {
        mainGauge = new Gauge(this);
        mainGauge->setValue(value); 
        frameLayout->addWidget(mainGauge, 0, Qt::AlignCenter);
    }

    else if (!has_bar) {

        bigBatteryWidget = new BigBatteryWidget(this);
        frameLayout->addWidget(bigBatteryWidget, 0, Qt::AlignCenter); 
    }

    if (has_bar) {

        progress_bar = new QProgressBar(this);
        progress_bar->setFixedSize(400, 20);
        progress_bar->setRange(0, qRound(value_limit.value(quantity)));
        progress_bar->setValue(qRound(value));
        progress_bar->setTextVisible(false);
        progress_bar->setStyleSheet(QString("QProgressBar { background-color: white; border-radius: 10px; } QProgressBar::chunk { background-color: %1; }").arg(THEME_COLOR));

        QGraphicsDropShadowEffect* barGlow = new QGraphicsDropShadowEffect(this);
        barGlow->setBlurRadius(15);
        barGlow->setColor(QColor(THEME_COLOR));
        barGlow->setOffset(0);
        progress_bar->setGraphicsEffect(barGlow);

        frameLayout->addWidget(progress_bar, 0, Qt::AlignCenter);
    }
}

void Containers::checkBoundsAndSetColor(double currentValue) {

    QString progressColor = THEME_COLOR;
    double compareValue = currentValue;

    if (quantity != "Throttle" && quantity != "RPM") {
        if (compareValue <= low_bound) {
            progressColor = low_color;
        }
        else if (compareValue < high_bound) {
            progressColor = mid_color;
        }
        else {
            progressColor = high_color;
        }
    }

    if (has_bar ) {
        progress_bar->setStyleSheet(QString(
            "QProgressBar { " "   background-color: #121212; " "   border-radius: 5px; "
            "   border: 1px solid #333; "
            "} "
            "QProgressBar::chunk { "
            "   background-color: %1; "
            "   border-radius: 5px; "
            "}"
        ).arg(progressColor));
    }
}

static double mapValue(double x, double in_min, double in_max, double out_min, double out_max) {
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void Containers::updateData(double newValue) {

    QString displayValue;
    value = newValue;
    
    displayValue = QString("%1 %2").arg(value, 0, 'f', 2).arg(unit);

    if (value_label) {
        value_label->setText(displayValue);
    }
   
    if (quantity == "Throttle" || quantity == "Power") {
        if(mainGauge) mainGauge->setValue(value);
    }

    else if (!has_bar) {
        double batt_pct = mapValue(value, m_guiMaster->min_batt_voltage, m_guiMaster->max_batt_voltage, 0.0, 100.0);
        bigBatteryWidget->setPercentage(batt_pct);
    }

    if (has_bar && progress_bar) {

        if (value >= value_limit.value(quantity)) progress_bar->setValue(qRound(value_limit.value(quantity)));
        else if (value <= 0) progress_bar->setValue(0);
        else progress_bar->setValue(qRound(value));
        
        checkBoundsAndSetColor(value);
    }

}

