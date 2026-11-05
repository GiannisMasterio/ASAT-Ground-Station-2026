#ifndef GCSGUI_H
#define GCSGUI_H

#include <QMainWindow>
#include <QTimer>
#include <QtSerialPort/QSerialPort>
#include <QFile>
#include "constants.h"
#include "mqttclient.h"
#include "containers.h"
#include "graphcontainer.h"
#include <qframe.h>
#include "qcustomplot.h"
#include "audiohandler.h"
#include "BatteryWidget.h"
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

    void* targetVar;

    enum Type { Int, Bool, String, Double } type;

    QLabel* label = nullptr;
    QWidget* entry = nullptr;

    SettingsElements(const QString& l, const QString& v, const QString& p, bool numeric, void* target,
        Type t) : labeltext(l), initialValue(v),
        placeholder(p),
        isNumeric(numeric),
        targetVar(target),
        type(t) {
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

    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }

    void setLatitude(double lat) {
        if (qFuzzyCompare(latitude, lat)) return;
        latitude = lat;
        emit latitudeChanged(); 
        emit locationChanged(); 
    }

    void setLongitude(double lon) {
        if (qFuzzyCompare(longitude, lon)) return;
        longitude = lon;
        emit longitudeChanged(); 
        emit locationChanged(); 
    }

    // STATIC METHOD
    static QString getHoverStyle(const QString& baseColor);

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

    void updateMqttData();
    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttMessageReceived(const QString& message);
    void publishMqttMessage(const QString& topic, const QString& payload);
    
    void setThrottle(qreal val);

    void close_program();
    void graphicUpdates();
    void chooseLogLocation();
    void openLogFile();
    void serial_control();
    void confirmPort();
    void changeScene();

    void chooseSettings();
    void saveSettings();

    void handleSerial(bool status);
    void readSerialData();

    void processData(const QString& data, QString source);
    void logToFile(bool status);

private:

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

    QPlainTextEdit* terminalLog = nullptr;
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

    // --- Strings & Collections ---
    const QStringList LOG_HEADERS = {
        "Timestamp", "Armed_State", "MainVoltage (V)", "MainCurrent (A)", "Power (W)", "ServoVoltage (V)", "ServoCurrent (A)", "Altitude (m)", "GroundSpeed (km/h)", "DistToHome (m)"
        ,"GPS_Latitude", "GPS_Longitude", "PiTemp (C)", "GPS_Sat_Count", "Throttle (%)", "RC_Motor (us)", "RC_Rudder (us)", "RC_Elevator (us)", "RC_Ail_L (us)", "RC_Ail_R (us)", "RC_Flap_L (us)", "RC_Flap_R (us)", "LandingGear1 (us)",
        "LandingGear2 (us)"
    };

    QString mqtt = "MQTT";
    QString serial = "SERIAL";
    QString selected_source = mqtt;
    QString com_port;
    QString scene_icon_path = PLANEICONPATH;
    QString status_icon_path = DISARMED_ICONPATH;
    QString logging_icon_path = LOGGING_OFF_ICONPATH;
    QString save_icon_path = SAVEICON_LOW_OPACITY_PATH;
    QString earth_icon_path = EARTH_ICONPATH;
    QString servo_icon_path = SERVOICONPATH;
    QString mqtt_receive_topic = DEFAULT_MQTT_RECEIVE_TOPIC;
    QString mqtt_send_topic = DEFAULT_MQTT_SEND_TOPIC;
    QString mqtt_broker = DEFAULT_MQTT_BROKER;
    QString log_file_path = "ASAT GCS Pro Records.csv";
    QString save_file_path = "ASAT GCS Pro Save.txt";
    QString current_scene = "MAIN";

    QStringList baudrates = { "300", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "57600", "115200" };
    QByteArray last_mqtt_data;

    QVector<SettingsElements> settings_fields;
    QVector<double> altitudeHistory; // buffer gia altitude graph
    QVector<double> timeHistory;

    // --- Control Surfaces Objects (OpenTX / EdgeTX Defaults) ---
    ControlSurfaces left_aileron{ 1, 988, 2012, 0, -45.0, 45.0, 0.0 };
    ControlSurfaces right_aileron{ 1, 988, 2012, 0, -45.0, 45.0, 0.0 }; // μπορεί να είναι το CH6 με mix, αλλά προς το παρόν ας το αφήσουμε 1
    ControlSurfaces elevator{ 2, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces motor{ 3, 988, 2012, 0 , -45.0, 45.0, 0.0 }; // Throttle
    ControlSurfaces rudder{ 4, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces left_flap{ 5, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces right_flap{ 5, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces landing_gear1{ 7, 988, 2012, 0 , 0.0, 90.0 , 0.0 };
    ControlSurfaces landing_gear2{ 8, 988, 2012, 0 , 0.0, 90.0 , 0.0 };
    ControlSurfaces arm_status_switch{ 9, 988, 2012,0 };

    // --- Doubles, qreal & 64-bit Integers (8 bytes) ---
    QElapsedTimer lastSceneChange;
    QElapsedTimer lastFunctionModeChange;
    qint64 lastLogTimestamp = 0;

    qreal m_throttle = 0;
    qreal m_leftAileronValue = 0.0;
    qreal m_rightAileronValue = 0.0;

    double main_batt_voltage = 0.0;
    double main_batt_current = 0.0;
    double servo_batt_voltage = 0.0;
    double servo_batt_current = 0.0;
    double altitude = 0.0;
    double latitude = 40.6401;  // ΠΡΟΕΠΙΛΕΓΩ ΘΕΣΣΑΛΟΝΙΚΗ
    double longitude = 22.9444;
    double homedist = 0.0;
    double ground_speed = 0.0;
    double pi_temp = 0.0;
    double current_throttle = 0.0;
    double current_power = 0.0;
    double graphtimeCounter = 0.0;
    double m_rudderValue = 0.0;
    double m_elevatorValue = 0.0;
    
    int m_cameraIndex = 0; // 0=Perspective, 1=Top, 2=Side, 3=Front
    int logIntervalMs = 500;
    int baudrate = 9600;
    int sat_count = 0;
    int auto_mode_timer_interval_ms = AUTO_MODE_TIMER_INTERVAL_MS;
    int mqtt_port = DEFAULT_MQTT_PORT;
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
    bool process_begin = false;
    bool unsaved_changes = false;
    bool armed = false;

    // --- Functions & Methods --- Aποθήκευση στο Text Segment ---
    double getServoAngle(ControlSurfaces* control_surface);
    void start_mqtt();
    void disconnect_mqtt();
    QFrame* createBottomToolbar();
    QWidget* createCameraWidget(int cameraIndex, QWidget* parent);
    void createMainScene();
    void createControlSurfacesScene();
    void createEarthProjectionScene();
    void setupSettingsFields();
    void loadSettings();
    void set_colors();
    void initLogFile();
    void setBatteryPercentage();
    void updateSurfaces();
    void updateBottomToolbar();
    void setup3DView(QGridLayout* layout);

    void updateSingleServoBox(QWidget* box, ControlSurfaces* surface);

	template<typename T> 
    bool updateValue(T& member, const T& newValue) { // θα εχουνε τον ιδιο τυπο
		if (member != newValue) {
			member = newValue;
			return true; // Ενημέρωσε το QML
		}
		return false; // Δεν άλλαξε τίποτα
    }

    template <>
	inline bool updateValue<qreal>(qreal& member, const qreal& newValue) { // αυτο ισχυει μονο  για qreal, γιατι ειναι float και δεν μπορουμε να χρησιμοποιησουμε το == με float
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