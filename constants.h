#pragma once
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>
#include <QMap>
#include <QIcon>
#include <QPair>
#include <QDir>
#include <QVector>

constexpr const char* INITIAL_RESOLUTION = "1920x1080";
const int INITIAL_RESOLUTION_ARR[2] = {1920, 1080};
constexpr const char* WINDOW_NAME = "Asat GCS Pro";

constexpr const char* ICONPATH = "./assets/AsatIcon.ico";
constexpr const char* ICONPATHPNG = "./assets/AsatIcon.png";
constexpr const char* ARMED_ICONPATH = "./assets/ArmedIcon2.png";
constexpr const char* DISARMED_ICONPATH = "./assets/DisarmedIcon2.png";
constexpr const char* CALIBRATIONICONPATH = "./assets/CalibrationIcon.png";
constexpr const char* SETTINGSICONPATH = "./assets/SettingsIcon.png";
constexpr const char* SAVEICONPATH = "./assets/SaveIcon.png";
constexpr const char* SAVEICON_LOW_OPACITY_PATH = "./assets/SaveIconLowOpacity.png";
constexpr const char* PROPELLERICONPATH = "./assets/PropellerIcon.png";
constexpr const char* PLANEICONPATH = "./assets/PlaneIcon.png";
constexpr const char* SERVOICONPATH = "./assets/ServoIcon.png";
constexpr const char* HOMEICONPATH = "./assets/HomeIcon.png";
constexpr const char* MAINSCENE_BG = "./assets/BackgroundMP1.png";
constexpr const char* LOGGING_ON_ICONPATH = "./assets/LogOn.png";
constexpr const char* LOGGING_OFF_ICONPATH = "./assets/LogOff.png";
constexpr const char* EARTH_ICONPATH = "./assets/EarthIcon.png";

constexpr const char* DEFAULT_MQTT_BROKER = "127.0.0.1";
constexpr const char* DEFAULT_MQTT_SEND_TOPIC = "thrust-toesp";
constexpr const char* DEFAULT_MQTT_RECEIVE_TOPIC = "daidalus/telemetry/state";
constexpr int DEFAULT_MQTT_PORT = 1883;

constexpr int MQTT_DELAY = 10; // ms
constexpr int GRAPHIC_UPDATES_REFRESH_RATE = 80; 

constexpr int MIN_THROTTLE = 1130; // ms
constexpr int MAX_THROTTLE = 2000; // ms

constexpr int RES_WIDTH = 1920;
constexpr int RES_HEIGHT = 1080;

constexpr const char* LOW_COLOR = "red";
constexpr const char* MID_COLOR = "orange";
constexpr const char* HIGH_COLOR = "green";

constexpr const char* SELECTED_GREEN = "limegreen";

constexpr const char* THEME_COLOR = "dodgerblue";
constexpr const char* HOME_BUTTON_COLOR = "blue";

constexpr const char* BORDER_COLOR = THEME_COLOR;
constexpr const char* CONTAINER_COLOR = "#363636";
constexpr const char* NEON_GREEN = "#64CC54";
constexpr const char* DARK_GRAY = "gray40";

constexpr int AUTO_MODE_TIMER_INTERVAL_MS = GRAPHIC_UPDATES_REFRESH_RATE;

const QMap<QString, QVector<QString>> progress_bar_colors_map = {
    {"Voltage",  {"red", "orange",SELECTED_GREEN}},
    {"Current",  {SELECTED_GREEN, "orange","red"}},
    {"RPM",      {"red", "orange", SELECTED_GREEN}},
    {"Throttle", {"red", "orange",SELECTED_GREEN}}
};

const QMap<QString, QPair<double, double>> bounds_map = {
    {"Voltage", {10.5, 12.9}},
    {"Current", {12.0, 20.0}},
    {"RPM", {3000.0, 8000.0}},
    {"Throttle", {4.0, 12.0}}
};

const QMap<QString, QString> units_map = {
    {"Instant RPM", "rpm"},
    {"Instant Throttle", "%"},
    {"MainBatt Voltage", "V"},
    {"MainBatt Current", "A"},
    {"SBatt Voltage", "V"},
    {"SBatt Current", "A"},
    {"Instant Power", "W"}
};

const QMap<QString, QString> actual_quantity = {
    {"Instant Power", "Power"},
    {"Instant Throttle", "Throttle"},
    {"MainBatt Voltage", "Voltage"},
    {"MainBatt Current", "Current"},
    {"SBatt Voltage", "Voltage"},
    {"SBatt Current", "Current"},
};

const QMap<QString, double> value_limit = {
    {"RPM", 10.0},
    {"Throttle", 100.0},
    {"Voltage", 12.9},
    {"Current", 35.0},
    {"Power", 20.0} 
};

inline const QString DIALOG_STYLE = QString::fromUtf8(R"(
QWidget#ModernDialog { background-color: #0b1220; }

QLabel { color: #e2e8f0; background: transparent; border: none; }
QLabel#DialogHeader  { color: #ffffff; }
QLabel#SectionLabel  { color: #94a3b8; }

QComboBox {
    background-color: rgba(255,255,255,14); color: #e2e8f0;
    border: 1px solid rgba(255,255,255,60); border-radius: 12px;
    padding: 4px 14px; min-height: 30px; font-size: 14px;
}
QComboBox:hover, QComboBox:focus { border-color: #1e90ff; }
QAbstractItemView {
    background-color: #131d33; color: #e2e8f0;
    border: 1px solid rgba(255,255,255,50);
    selection-background-color: #1e90ff; selection-color: white; outline: none;
}

QLineEdit {
    background-color: rgba(255,255,255,14); color: white;
    border: 1px solid rgba(255,255,255,60); border-radius: 10px;
    padding: 4px 10px; min-height: 28px; font-size: 14px;
}
QLineEdit:focus { border-color: #1e90ff; }

QSlider::groove:horizontal { height: 6px; background: rgba(255,255,255,30); border-radius: 3px; }
QSlider::sub-page:horizontal { background: #1e90ff; border-radius: 3px; }
QSlider::handle:horizontal { background: white; width: 18px; height: 18px; margin: -6px 0; border-radius: 9px; }
QSlider::handle:horizontal:hover { background: #38bdf8; }

QPushButton {
    background-color: rgba(255,255,255,12); color: #e2e8f0;
    border: 1px solid rgba(255,255,255,70); border-radius: 22px;
    font-size: 15px; font-weight: 600; padding: 0 20px;
}
QPushButton:hover   { background-color: rgba(30,144,255,60); border-color: #1e90ff; color: white; }
QPushButton:pressed { background-color: rgba(30,144,255,110); }
QPushButton#PrimaryButton { background-color: #1e90ff; border: none; color: white; }
QPushButton#PrimaryButton:hover   { background-color: #38bdf8; }
QPushButton#PrimaryButton:pressed { background-color: #0369a1; }
QPushButton:disabled {
    background-color: rgba(255,255,255,6); color: rgba(226,232,240,90);
    border: 1px solid rgba(255,255,255,25);
}
QTabWidget::pane {
    border: 1px solid rgba(255,255,255,40); border-radius: 16px;
    background-color: rgba(255,255,255,8); top: -1px;
}
QTabBar { background: transparent; }
QTabBar::tab {
    background: transparent; color: #94a3b8; border: none;
    border-bottom: 2px solid transparent;
    padding: 10px 18px; margin-right: 4px; font-size: 14px; font-weight: 600;
}
QTabBar::tab:hover:!selected { color: #e2e8f0; }
QTabBar::tab:selected { color: white; border-bottom: 2px solid #1e90ff; }

QCheckBox { color: #e2e8f0; font-size: 14px; spacing: 10px; background: transparent; }
QCheckBox::indicator {
    width: 20px; height: 20px; border-radius: 6px;
    border: 2px solid rgba(255,255,255,90); background: transparent;
}
QCheckBox::indicator:hover   { border-color: #1e90ff; }
QCheckBox::indicator:checked { background-color: #1e90ff; border-color: #1e90ff; }

QPushButton:disabled {
    background-color: rgba(255,255,255,6); color: rgba(226,232,240,90);
    border: 1px solid rgba(255,255,255,25);
}

QMessageBox { background-color: #0b1220; }
QMessageBox QPushButton { min-width: 90px; min-height: 36px; }
)" 
);

#endif 