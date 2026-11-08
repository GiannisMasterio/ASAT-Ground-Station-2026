#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <QWidget>
#include <QFrame>
#include "hudwidgets.h"
class QLabel;
class QProgressBar;
class QCheckBox;
class QPushButton;
class GcsGUI;

class Containers : public QFrame
{
    Q_OBJECT

public:
    explicit Containers(const QString& title, int titleSize, const QString& titleColor,
        double initialValue, bool hasBar,  GcsGUI* guiMaster,
        QWidget* parent = nullptr);


    ~Containers() override = default;


public slots:
    void updateData(double newValue);

private:
    QString title_text;
    int title_label_size;
    QString title_label_color;
    double value;
    QString unit;
    bool has_bar;
    QString quantity;

    double low_bound = 2.0;
    double high_bound = 5.0;

    QString low_color;
    QString mid_color;
    QString high_color;

    QLabel* title_label;
    QLabel* value_label = nullptr;
    QProgressBar* progress_bar = nullptr;

    QWidget* control_frame = nullptr;
    QCheckBox* toggle_switch = nullptr;
    QPushButton* slider_opener_button = nullptr;

    GcsGUI* m_guiMaster;
    
    Gauge* mainGauge = nullptr; 
    BigBatteryWidget* bigBatteryWidget = nullptr;

    void checkBoundsAndSetColor(double currentValue);
    void design();
};

#endif 
