#pragma once
#ifndef BACKENDHANDLER_H
#define BACKENDHANDLER_H

#include <random>
#include "constants.h"
#include "AudioHandler.h"
#include <QSerialPort>
#include "VarState.h"

class QWidget;
class QPushButton;
class QSlider;
class QLabel;
class QDialog;
class QPlainTextEdit;

class BackendHandler : public QObject
{
    Q_OBJECT

public:

    explicit BackendHandler(QObject* parent, QObject* defaultValue);

    ~BackendHandler() override = default;

    QSlider* slider;
    QPushButton* slider_opener_button = nullptr;
    QMap<QString, QPair<unsigned int, unsigned int>> mode_value_limits;

    TelemetryData tel;
    MqttConfig mqttcfg;
	CalibrationConfig cal;
    PlaneControlSurfaces surfaces;

    AudioHandler* audio_handler = nullptr;

    void playAudio(int volume, QString audio_tag);
    bool isSoundPlaying(QString audio);
    void setSoundStatus(bool sound_status);


    void start_mqtt();
    void disconnect_mqtt();
    void change_mqtt_topic();

    QStringList getAvailablePorts();
    bool connectSerial(const QString& portName, int baudrate);
    bool isSerialConnected() const;

    void initLogFile();
    void logToFile(bool status);

    double getThrottlePercentage(int current_throttle);

    double mapValue(double x, double in_min, double in_max, double out_min, double out_max);
        

    QTimer* auto_mode_timer = nullptr;

    QStringList baudrates = { "300", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "57600", "115200" };

    QString selected_propeller;

    bool experiment_running = false;
    bool log_only_during_experiment = true;
    bool experiment_is_transitioning = false;
    bool experiment_is_static = false;

    int audio_click_sfx_volume = 70;

    QString log_file_path = "ASAT ThrustStand Records.csv";
    QString save_file_path = "ASAT ThrustStandSmart Save.txt";
    QFile* log_file = nullptr;
    QFile* save_file = nullptr;

    QString selected_data_source = "MQTT";

    int logIntervalMs = 500;

    bool running = true;
    bool process_begin = false;
    bool unsaved_settings_changes = false;
    bool is_armed = false;
    bool soundOn = true;

    bool write_logs = false;

    bool use_rpm_filter = false;
    QVector<double> rpm_buffer; //used for rpm filtering
    int rpm_filter_window = 8;

    QSerialPort* serial_port = nullptr;
    QString serial_com_port_tag;

    int serial_baudrate = 9600;

    QTimer* simulator_timer = nullptr;
    double sim_actual_rpm = 0.0;
    double sim_actual_airspeed = 0.0;
    std::mt19937 rng{ std::random_device{}() };

    QString com_port;
    QPlainTextEdit* terminalLog = nullptr;

protected:

    QDateTime stage3_start_time;
    QDateTime record_data_wait_time;
    QDateTime initial_delay_start_time;

    bool record_data_wait_time_initialized = false;
    bool initial_delay_initialized = false;

    QVector<double> thrustSampleBuffer;
    QVector<double> transitionSampleBuffer;

    const int MONITOR_DURATION_MS = 3000;

    double slider_stored_value;

    double lower_rpm_bound = 0;
    double upper_rpm_bound = 0;

    QString mode;

    QDialog* slider_window = nullptr;
    QLabel* slider_value_label = nullptr;

private:
    QString function_mode = "Basic";
    bool experiment_just_started = true;
    bool max_throttle_reached = false;
    bool stabilizing_thrust = false;
    bool target_rpm_reached = false;
    bool ending_transition = false;

    bool stabilizing_transition = false;

    double battery_removed_voltage_value = 204.8;

    int transition_stab_wait_time_ms = 3000;
    int baudrate = 115200;

    QTimer* mqtt_data_timer = nullptr;

    double RPMFilter(double new_rpm);

signals:
    void telemetryParsed(QMap<QString, double> parsedValues);
    void publishMqtt(const QString& topic, const QString& payload);
    void modeChanged(const QString& newMode);

    void experimentFinished();

    void mqttSendTopicChanged(const QString& newTopic);

    void telemetryUpdated();
    void armedStateChanged(bool armed);
    void loggingStateChanged(bool logging);

public slots:
    void receiveTelemetry(const QString& data, QString source);
    void updateTelemetry(QMap<QString, double> value_keeper);
    void updateMqttSendTopic(const QString& newTopic);

    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttMessageReceived(const QString& message);

    void updateMqttData();
    void chooseLogLocation();
    void openLogFile();

    void confirmPort();
    void serial_sendCommand(QString command);
    void handleSerial(bool status);
    void readSerialData();

    void publishMqttMessage(const QString& topic, const QString& payload);
};

#endif
