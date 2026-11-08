#ifndef GCSGUI_H
#define GCSGUI_H

#include <QMainWindow>
#include <QTimer>
#include <QtSerialPort/QSerialPort>
#include <QFile>
#include "Constants.h"
#include "MqttClient.h"
#include "Containers.h"
#include "GraphContainer.h"
#include <qframe.h>
#include "BatteryWidget.h"
#include "BackendHandler.h"
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointLight>
#include <QGridLayout>
#include <QtQuickWidgets/QQuickWidget>
#include <QQmlContext>
#include <QStackedWidget>
#include <QQuickItem>
#include <QQmlEngine>
#include <QPlainTextEdit>
#include <QPainter>
#include <QWidget>

struct SettingsElements {
    QString labeltext;
    QString initialValue;
    QString placeholder;
    bool isNumeric = false;

    void* targetVar; // Βεβαιωθείτε ότι υπάρχει το αστεράκι (*)

    enum Type { Int, Bool, String, Double } type;

    QLabel* label = nullptr;
    QWidget* entry = nullptr;

    // Ο Constructor πρέπει να είναι ακριβώς έτσι:
    SettingsElements(const QString& l, const QString& v, const QString& p, bool numeric, void* target, Type t)
        : labeltext(l), initialValue(v), placeholder(p), isNumeric(numeric), targetVar(target), type(t)
    {
    }
};

struct GraphDataPoint {
    double time;
    double value;
};

struct LogEntry {
    double timeInSeconds;
    double main_batt_voltage, main_batt_current, servo_batt_voltage, servo_batt_current, power, throttle, altitude, latitude, longitude;
    double homedist, ground_speed;
    int sat_count = 0;
};

enum IconType { Scene, Logging, Earth, Status,  Settings, Save, Battery};

class Containers;
class Backendhandler;

// Σε αντίθεση με γλώσσες όπως η C# ή η Java, η C++ δεν διαβάζει όλα τα αρχεία του project πρώτα για να δει "ποιες κλάσεις υπάρχουν".
// Όταν ο compiler ανοίγει να διαβάσει το GcsGUI.h και φτάνει στη γραμμή 236, βλέπει μια λέξη (Backendhandler) την οποία δεν έχει ξανασυναντήσει
// μέχρι εκείνη τη γραμμή. Αφού δεν ξέρει τι είναι (δεν ξέρει αν είναι μεταβλητή, συνάρτηση ή κλάση), πετάει σφάλμα.

class GcsGUI : public QMainWindow
{
    Q_OBJECT

        // === 1. RUDDER ===
        Q_PROPERTY(QColor rudderColor READ rudderColor WRITE setRudderColor NOTIFY rudderColorChanged)
        Q_PROPERTY(double rudderValue READ rudderValue WRITE setRudderValue NOTIFY rudderValueChanged)

        // === 2. ELEVATOR ===
        Q_PROPERTY(QColor elevatorColor READ elevatorColor WRITE setElevatorColor NOTIFY elevatorColorChanged)
        Q_PROPERTY(double elevatorValue READ elevatorValue WRITE setElevatorValue NOTIFY elevatorValueChanged)

        // === 4. AILERONS === 
        Q_PROPERTY(QColor leftAileronColor READ leftAileronColor WRITE setLeftAileronColor NOTIFY leftAileronColorChanged)
        Q_PROPERTY(QColor rightAileronColor READ rightAileronColor WRITE setRightAileronColor NOTIFY rightAileronColorChanged)

        Q_PROPERTY(qreal leftAileronValue READ leftAileronValue WRITE setLeftAileronValue NOTIFY leftAileronValueChanged)
        Q_PROPERTY(qreal rightAileronValue READ rightAileronValue WRITE setRightAileronValue NOTIFY rightAileronValueChanged)

        // === 5. FLAPS ===
        Q_PROPERTY(QColor leftFlapsColor READ leftFlapsColor WRITE setLeftFlapsColor NOTIFY leftFlapsColorChanged)
        Q_PROPERTY(QColor rightFlapsColor READ rightFlapsColor WRITE setRightFlapsColor NOTIFY leftFlapsColorChanged)

        Q_PROPERTY(qreal throttle READ throttle WRITE setThrottle NOTIFY throttleChanged)

        Q_PROPERTY(int cameraIndex READ cameraIndex WRITE setCameraIndex NOTIFY cameraIndexChanged)

        Q_PROPERTY(double latitude READ getLatitude WRITE setLatitude NOTIFY latitudeChanged)
        Q_PROPERTY(double longitude READ getLongitude WRITE setLongitude NOTIFY longitudeChanged)

public:

    explicit GcsGUI(QWidget* parent = nullptr);
    ~GcsGUI() override;

    int cameraIndex() const { return m_cameraIndex; }
    void setCameraIndex(int index);

    qreal throttle() const { return m_throttle; }

    QColor rudderColor() const { return m_rudderColor; }
    double rudderValue() const { return m_rudderValue; }

    QColor leftAileronColor() const { return m_leftAileronColor; }
    QColor rightAileronColor() const { return m_rightAileronColor; }

    qreal leftAileronValue() const;
    void setLeftAileronValue(qreal value);

    qreal rightAileronValue() const;
    void setRightAileronValue(qreal value);

    QColor leftFlapsColor() const { return m_leftFlapsColor; }
    QColor rightFlapsColor() const { return m_rightFlapsColor; }

    void setLeftAileronColor(const QColor& color);
    void setRightAileronColor(const QColor& color);

    void setLeftFlapsColor(const QColor& color) ;
    void setRightFlapsColor(const QColor& color) ;

    QColor elevatorColor() const { return m_elevatorColor; }
    double elevatorValue() const { return m_elevatorValue; }

    void setRudderColor(const QColor& color);
    void setRudderValue(double value);

    void setElevatorColor(const QColor& color);
    void setElevatorValue(double value);

    double getLatitude() const;
    double getLongitude() const;
    void setLatitude(double lat);
    void setLongitude(double lon);

    // USEFUL POINTERS
    AudioHandler* audio_handler = nullptr;
    QTimer* auto_mode_timer = nullptr;

    double min_batt_voltage = 9.0;
    double max_batt_voltage = 12.6;  // 9.0V άδεια - 12.6V γεμάτη
    double max_expected_amps = 30.0;

    int audio_click_sfx_volume = 100;

    // FLAGS
    bool soundOn = true;
    bool write_logs = false;
signals:

    void locationChanged();
    void mqttSendTopicChanged(const QString& newTopic);

    void latitudeChanged();
    void longitudeChanged();

    void cameraIndexChanged();
    void throttleChanged();
    
    void rudderColorChanged();
    void rudderValueChanged();

    void leftAileronColorChanged();
    void rightAileronColorChanged();

    void leftAileronValueChanged();
    void rightAileronValueChanged();

    void leftFlapsColorChanged();
    void rightFlapsColorChanged();

    void elevatorColorChanged();
    void elevatorValueChanged();

private slots:

    void updateUIFromTelemetry(QMap<QString, double> values);
    void setThrottle(qreal val);

    void close_program();
    void graphicUpdates();

    void serialControlWindow();
    void changeScene();

    void chooseSettings();
    void saveSettings();

private:
	BackendHandler* backend_handler = nullptr;

    QLabel* latitudeLabel = nullptr;
    QLabel* longitudeLabel = nullptr;
    QLabel* satCountLabel = nullptr;
    QLabel* linkQualityLabel = nullptr;
    QLabel* gSpeedLabel = nullptr;
    QLabel* distToHomeLabel = nullptr;
    QLabel* piTempLabel = nullptr;

    BatteryWidget* batteryIndicator = nullptr;

    QTimer* mqtt_data_timer = nullptr;
    QTimer* graphic_timer = nullptr;

    QQmlEngine* sharedQmlEngine = nullptr;

    QStackedWidget* sceneStack = nullptr;
    QWidget* mainSceneWidget = nullptr;
    QWidget* controlSurfacesSceneWidget = nullptr;
    QWidget* EarthProjectionSceneWidget = nullptr;
    QWidget* m_3dContainer = nullptr;

    QWidget* aileronLBox = nullptr;
    QWidget* aileronRBox = nullptr;
    QWidget* elevatorBox = nullptr;
    QWidget* rudderBox = nullptr;
    QWidget* flapsBox = nullptr;
    QWidget* lgear1Box = nullptr;
    QWidget* lgear2Box = nullptr;

    QSerialPort* serial_port = nullptr;

    QPushButton* sceneIconButton = nullptr;
    QPushButton* statusIconButton = nullptr;
    QPushButton* loggingIconButton = nullptr;
    QPushButton* saveIconButton = nullptr;
    QPushButton* earthIconButton = nullptr;

    Gauge* mainGauge = nullptr;
    GraphContainer* altitudeGraph = nullptr;

    Containers* servo_batt_voltageGUI = nullptr;
    Containers* servo_batt_currentGUI = nullptr;
    Containers* main_batt_currentGUI = nullptr;
    Containers* main_batt_voltageGUI = nullptr;
    Containers* instantthrottleGUI = nullptr;
    Containers* instantpowerGUI = nullptr;

    MqttClient* mqtt_client = nullptr;
    QFile* log_file = nullptr;
    QFile* save_file = nullptr;

    double graphtimeCounter = 0.0;

    // --- Strings & Collections ---
    const QStringList LOG_HEADERS = {
        "Timestamp", "Armed_State", "MainVoltage (V)", "MainCurrent (A)", "Power (W)", "ServoVoltage (V)", "ServoCurrent (A)", "Altitude (m)", "GroundSpeed (km/h)", "DistToHome (m)"
        ,"GPS_Latitude", "GPS_Longitude", "PiTemp (C)", "GPS_Sat_Count", "Throttle (%)", "RC_Motor (us)", "RC_Rudder (us)", "RC_Elevator (us)", "RC_Ail_L (us)", "RC_Ail_R (us)", "RC_Flap_L (us)", "RC_Flap_R (us)", "LandingGear1 (us)",
        "LandingGear2 (us)"
    };

    QString mqtt = "MQTT";
    QString serial = "SERIAL";
    QString selected_source = mqtt;
    
    QString scene_icon_path = PLANEICONPATH;
    QString status_icon_path = DISARMED_ICONPATH;
    QString logging_icon_path = LOGGING_OFF_ICONPATH;
    QString save_icon_path = SAVEICON_LOW_OPACITY_PATH;
    QString earth_icon_path = EARTH_ICONPATH;
    QString servo_icon_path = SERVOICONPATH;
  
    QString save_file_path = "ASAT GCS Pro Save.txt";
    QString current_scene = "MAIN";

    QByteArray last_mqtt_data;

    QVector<SettingsElements> settings_fields;
    QVector<double> altitudeHistory; // buffer gia altitude graph
    QVector<double> timeHistory;

    // --- Doubles, qreal & 64-bit Integers (8 bytes) ---
    QElapsedTimer lastSceneChange;
    QElapsedTimer lastFunctionModeChange;
    qint64 lastLogTimestamp = 0;

    qreal m_throttle = 0;
    qreal m_leftAileronValue = 0.0;
    qreal m_rightAileronValue = 0.0;

    double m_rudderValue = 0.0;
    double m_elevatorValue = 0.0;
    
    int m_cameraIndex = 0; // 0=Perspective, 1=Top, 2=Side, 3=Front
    int logIntervalMs = 500;


    int auto_mode_timer_interval_ms = AUTO_MODE_TIMER_INTERVAL_MS;

    const int MAX_GRAPH_POINTS = 2000;
    const int max_motor_throttle = 200; // edw antistoixei se timh toy qml

    QColor m_rudderColor = Qt::white;
    QColor m_leftAileronColor = Qt::white;
    QColor m_rightAileronColor = Qt::white;
    QColor m_leftFlapsColor = Qt::white;
    QColor m_rightFlapsColor = Qt::white;
    QColor m_elevatorColor = Qt::white;
    QColor m_motorColor = Qt::white;

    // --- Booleans (1 byte) ---
    bool move_control_surfaces = true;
    bool wireless_source = true;
    bool serial_source = true;
    bool running = true;
    bool process_begin = true;
    bool unsaved_changes = false;
    bool armed = false;

    // --- Functions & Methods --- Aποθήκευση στο Text Segment ---
    double getServoAngle(ControlSurfaces* control_surface);
    QFrame* createBottomToolbar();
    QWidget* createCameraWidget(int cameraIndex, QWidget* parent);

    void createMainScene();
    void createControlSurfacesScene();
    void createEarthProjectionScene();

    void setupSettingsFields();
    void loadSettings();
    void set_colors();

    void setBatteryPercentage();
    void updateSurfaces();
    void updateBottomToolbar();
    void setup3DView(QGridLayout* layout);

    void updateSingleServoBox(QWidget* box, ControlSurfaces* surface);

    template<typename T>
    bool updateValue(T& member, const T& newValue) {
        if (member != newValue) {
            member = newValue;
            return true;
        }
        return false;
    }

    // ΑΠΛΟ OVERLOAD - ΟΧΙ TEMPLATE SPECIALIZATION
    inline bool updateValue(qreal& member, const qreal& newValue) {
        if (qFuzzyCompare(member, newValue)) {
            return false;
        }
        member = newValue;
        return true;
    }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
};
#endif