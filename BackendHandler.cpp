#include "BackendHandler.h"
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QVBoxLayout>
#include <QSlider>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QUrl>
#include <algorithm>
#include "MqttClient.h"
#include "Containers.h"
#include "GraphContainer.h"
#include <QFrame>
#include <QtSerialPort/QSerialPortInfo>
#include "VarState.h"

double mapValue(double x, double in_min, double in_max, double out_min, double out_max) {
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

using namespace std;

BackendHandler::BackendHandler(QObject* parent, QObject* defaultValue) :
    QObject(parent)
{
    serial_port = new QSerialPort(this);
    mqtt_data_timer = new QTimer(this);

    initLogFile();

    connect(this, &BackendHandler::mqttSendTopicChanged, this, &BackendHandler::updateMqttSendTopic);
    connect(this, &BackendHandler::publishMqtt, this, &BackendHandler::publishMqttMessage);
    connect(mqtt_data_timer, &QTimer::timeout, this, &BackendHandler::updateMqttData);

    mqtt_data_timer->start(MQTT_DELAY);
    start_mqtt();

    audio_handler = new AudioHandler(this);
}

// Telemetry Functions
void BackendHandler::receiveTelemetry(const QString& data, QString source) {
    if (source != selected_data_source) return;

    QString cleanData = data.trimmed().replace(" ", "");
    QMap<QString, double> value_keeper;
    QStringList parts = cleanData.split(",");

    for (const QString& part : parts) {
        QStringList pair = part.split(":");
        if (pair.size() == 2) {
            bool ok;
            double val = pair[1].toDouble(&ok);
            if (ok) {
                value_keeper[pair[0]] = val;
            }
        }
    }

    if (!value_keeper.isEmpty()) {
        updateTelemetry(value_keeper);
    }
}

void BackendHandler::updateTelemetry(QMap<QString, double> value_keeper) {
    auto& tel = this->tel;

    tel.main_batt_voltage = value_keeper.value("MV", tel.main_batt_voltage);
    tel.main_batt_current = value_keeper.value("MC", tel.main_batt_current);
    tel.servo_batt_voltage = value_keeper.value("SV", tel.servo_batt_voltage);
    tel.servo_batt_current = value_keeper.value("SC", tel.servo_batt_current);
    tel.ground_speed = value_keeper.value("GS", tel.ground_speed);
    tel.altitude = value_keeper.value("ALT", tel.altitude);
    tel.latitude = value_keeper.value("LAT", tel.latitude);
    tel.longitude = value_keeper.value("LON", tel.longitude);
    tel.homedist = value_keeper.value("HD", tel.homedist);
    tel.sat_count = value_keeper.value("SC", tel.sat_count);
    tel.pi_temp = value_keeper.value("PT", tel.pi_temp);

    tel.current_power = tel.main_batt_voltage * tel.main_batt_current; // Προσοχή: power, όχι current_power σύμφωνα με το VarState.h

    // Ειδοποιούμε το GUI (GcsGUI) ότι τα δεδομένα στο tel ενημερώθηκαν!
    emit telemetryParsed(value_keeper);
}

// LOGGING FUNCTIONS
void BackendHandler::chooseLogLocation() {
    QString initialDir = QFileInfo(log_file_path).path();
    QString newFilePath = QFileDialog::getSaveFileName(
        nullptr, // ΔΙΟΡΘΩΣΗ: nullptr αντί για this (επειδή το BackendHandler δεν είναι QWidget)
        "Select Log File Location",
        initialDir,
        "CSV Log (*.csv);;Text Log (*.txt);;All files (*.*)"
    );

    if (!newFilePath.isEmpty()) {
        if (!newFilePath.endsWith(".csv", Qt::CaseInsensitive) && !newFilePath.endsWith(".txt", Qt::CaseInsensitive)) {
            newFilePath += ".csv";
        }

        log_file_path = newFilePath;
        initLogFile();
        qDebug() << "New log file location set to:" << log_file_path;
    }
}

void BackendHandler::initLogFile() {
    QString fullPath = log_file_path;

    if (!QFileInfo(fullPath).isAbsolute() || fullPath.isEmpty()) {
        fullPath = QCoreApplication::applicationDirPath() + QDir::separator() + log_file_path;
    }

    QFile file(fullPath);

    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "                     Timestamp                  Thrust (N)               Torque (N*m)               Voltage (V)                Current (A)               RPM               Throttle (%)                  Power (W)              Propeller Name            AOA (deg)           Airspeed (m/s)\n";
            file.close();
            qDebug() << "Created initial log file:" << fullPath;
        }
    }

    log_file_path = fullPath;
}

void BackendHandler::logToFile(bool status) {
    if (!status || !write_logs) return;

    QFile file(log_file_path);
    bool write_header = (!file.exists() || file.size() == 0);

    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Error opening log file:" << file.errorString();
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    // ΔΥΝΑΜΙΚΗ ΛΙΣΤΑ LOGGING (Τηλεμετρία + Όλα τα PWM)
    QVector<QPair<QString, QString>> log_columns = {
        { "Timestamp", timestamp },
        { "MainBatt_V", QString::number(tel.main_batt_voltage, 'f', 2) },
        { "MainBatt_A", QString::number(tel.main_batt_current, 'f', 2) },
        { "Power_W", QString::number(tel.current_power, 'f', 2) },
        { "ServoBatt_V", QString::number(tel.servo_batt_voltage, 'f', 2) },
        { "ServoBatt_A", QString::number(tel.servo_batt_current, 'f', 2) },
        { "Altitude_m", QString::number(tel.altitude, 'f', 2) },
        { "GroundSpeed_kmh", QString::number(tel.ground_speed, 'f', 2) },
        { "HomeDist_m", QString::number(tel.homedist, 'f', 2) },
        { "Latitude", QString::number(tel.latitude, 'f', 6) },
        { "Longitude", QString::number(tel.longitude, 'f', 6) },
        { "PiTemp_C", QString::number(tel.pi_temp, 'f', 1) },
        { "Sat_Count", QString::number(tel.sat_count) },
        { "Throttle_Pct", QString::number(getThrottlePercentage(surfaces.motor.current_value), 'f', 2) },
        { "PWM_Motor", QString::number(surfaces.motor.current_value) },
        { "PWM_Rudder", QString::number(surfaces.rudder.current_value) },
        { "PWM_Elevator", QString::number(surfaces.elevator.current_value) },
        { "PWM_Aileron_L", QString::number(surfaces.left_aileron.current_value) },
        { "PWM_Aileron_R", QString::number(surfaces.right_aileron.current_value) },
        { "PWM_Flap_L", QString::number(surfaces.left_flap.current_value) },
        { "PWM_Flap_R", QString::number(surfaces.right_flap.current_value) },
        { "PWM_Gear_1", QString::number(surfaces.landing_gear1.current_value) },
        { "PWM_Gear_2", QString::number(surfaces.landing_gear2.current_value) },
        { "PWM_ArmSwitch", QString::number(surfaces.arm_status_switch.current_value) }
    };

    QTextStream out(&file);

    // Γράφει αυτόματα τις επικεφαλίδες αν το αρχείο είναι άδειο (με διαχωριστικό το ερωτηματικό ;)
    if (write_header) {
        QStringList headers;
        for (const auto& col : log_columns) headers << col.first;
        out << headers.join(";") << "\n";
    }

    // Γράφει τις τιμές (με διαχωριστικό το ερωτηματικό ;)
    QStringList values;
    for (const auto& col : log_columns) values << col.second;
    out << values.join(";") << "\n";

    file.close();
}

void BackendHandler::openLogFile() {
    QString fullFilePath = log_file_path;

    if (!QFileInfo(fullFilePath).isAbsolute() || fullFilePath.isEmpty()) {
        fullFilePath = QCoreApplication::applicationDirPath() + QDir::separator() + "ASAT ThrustStand Records.csv";
    }

    if (!QFile::exists(fullFilePath)) {
        QMessageBox::warning(nullptr, "File Not Found", "Log file does not exist at: " + fullFilePath);
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(fullFilePath))) {
        QMessageBox::critical(nullptr, "Error", "Could not open file with the system default application.");
    }
}

// MQTT Functions

void BackendHandler::start_mqtt() {
    if (mqttcfg.mqtt_client) {
        mqttcfg.mqtt_client->disconnectFromBroker();
        delete mqttcfg.mqtt_client; // διαγραφη παλιου reference
        mqttcfg.mqtt_client = nullptr;
    }
    mqttcfg.mqtt_client = new MqttClient(mqttcfg.mqtt_broker, mqttcfg.mqtt_port, mqttcfg.mqtt_receive_topic, mqttcfg.mqtt_send_topic, this);

    connect(mqttcfg.mqtt_client, &MqttClient::onConnected, this, &BackendHandler::onMqttConnected);
    connect(mqttcfg.mqtt_client, &MqttClient::onDisconnected, this, &BackendHandler::onMqttDisconnected);
    connect(mqttcfg.mqtt_client, &MqttClient::on_mqtt_message, this, &BackendHandler::onMqttMessageReceived);

    mqttcfg.mqtt_client->connectToBroker();
}

void BackendHandler::publishMqttMessage(const QString& topic, const QString& payload) {
    if (mqttcfg.mqtt_client) {
        qint32 id = mqttcfg.mqtt_client->publish(topic, payload);
        if (id != -1) {
            qDebug() << "MQTT PUBLISHED via Wrapper: Topic=" << topic << ", Payload=" << payload << ", ID=" << id;
        }
        else {
            qWarning() << "ERROR publishing via Wrapper. Client state is likely disconnected.";
        }
    }
    else {
        qWarning() << "MQTT Client Wrapper not initialized. Message not sent.";
    }
}

void BackendHandler::disconnect_mqtt() {
    auto& mqtt_client = mqttcfg.mqtt_client;

    if (mqtt_client == nullptr) {
        qDebug() << "MQTT client was never initialized.";
        return;
    }

    if (mqtt_client->getState() == QMqttClient::Connected) {
        mqtt_client->unsubscribe(QMqttTopicFilter(mqttcfg.mqtt_receive_topic));
        mqtt_client->unsubscribe(QMqttTopicFilter(mqttcfg.mqtt_send_topic));
        mqtt_client->disconnectFromHost();
        qDebug() << "Unsubscribed and requested disconnection from MQTT Broker.";
    }
}

void BackendHandler::change_mqtt_topic() {
    disconnect_mqtt();
    updateMqttSendTopic(mqttcfg.mqtt_send_topic);
    start_mqtt();
}

void BackendHandler::onMqttConnected() {
    qDebug() << "Connected to MQTT Broker:" << mqttcfg.mqtt_broker << ":" << mqttcfg.mqtt_port;
}

void BackendHandler::onMqttDisconnected() {
    qDebug() << "Disconnected from MQTT Broker.";
}

void BackendHandler::onMqttMessageReceived(const QString& data) {
    if (!process_begin) {
        process_begin = true;
        playAudio(100, "TelemetryConnected");
    }

    mqttcfg.last_mqtt_data = data.toUtf8();
    qDebug() << "received (MQTT String Data):" << data;
}

void BackendHandler::updateMqttData() {
    if (!mqttcfg.last_mqtt_data.isEmpty()) {
        receiveTelemetry(QString::fromUtf8(mqttcfg.last_mqtt_data), "MQTT");
        mqttcfg.last_mqtt_data.clear();
    }
}

void BackendHandler::updateMqttSendTopic(const QString& newTopic) {
    mqttcfg.mqtt_send_topic = newTopic;
    qDebug() << "ModeHandler MQTT Topic updated to:" << newTopic;
}

// SOUND HANDLER FUNCTIONS

void BackendHandler::playAudio(int volume, QString audio_tag) {
    if (audio_handler) audio_handler->playSound(volume, audio_tag);
}

bool BackendHandler::isSoundPlaying(QString audio) {
    return audio_handler ? audio_handler->isSoundPlaying(audio) : false;
}

void BackendHandler::setSoundStatus(bool sound_status) {
    if (audio_handler) {
        audio_handler->setSoundStatus(sound_status);
    }
}

// OTHER
double BackendHandler::getThrottlePercentage(int current_throttle) {

    if (cal.max_throttle <= cal.min_throttle)
        return 0.0;

    return 100.0 * (current_throttle - cal.min_throttle) / (cal.max_throttle - cal.min_throttle);
}

double BackendHandler::RPMFilter(double new_rpm) {
    rpm_buffer.push_back(new_rpm);

    if (rpm_buffer.size() > rpm_filter_window) {
        rpm_buffer.erase(rpm_buffer.begin());
    }

    QVector<double> sorted_buffer = rpm_buffer;
    std::sort(sorted_buffer.begin(), sorted_buffer.end());

    return sorted_buffer[sorted_buffer.size() / 2];
}

// SERIAL CONTROL

void BackendHandler::handleSerial(bool status) {

    if (status) {

        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Closed previous serial port.";
        }

        serial_port->setPortName(com_port);
        serial_port->setBaudRate(baudrate);

        if (serial_port->open(QIODevice::ReadWrite)) {
            connect(serial_port, &QSerialPort::readyRead, this, &BackendHandler::readSerialData);
            qDebug() << "Opened serial port" << com_port << "at" << baudrate;
        }

        else {
            qWarning() << "Cannot open port" << com_port << ":" << serial_port->errorString();
        }
    }

    else {

        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Serial port closed.";
        }

    }
}

void BackendHandler::readSerialData() {
    while (serial_port->canReadLine()) {
        QByteArray data = serial_port->readLine();
        if (!data.isEmpty()) {
            QString serialData = QString::fromUtf8(data).trimmed();
            if (!process_begin) {
                process_begin = true;
            }
            // ΟΧΙ terminalLog εδώ
            receiveTelemetry(serialData, "SERIAL");
        }
    }
}

void BackendHandler::confirmPort() {
    qDebug() << "Selected COM port:" << com_port;
    qDebug() << "Selected baudrate:" << baudrate;
    handleSerial(running);
}

QStringList BackendHandler::getAvailablePorts() {
    QStringList ports;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    return ports;
}

bool BackendHandler::connectSerial(const QString& portName, int baudrate) {
    serial_com_port_tag = portName;
    serial_baudrate = baudrate;

    playAudio(audio_click_sfx_volume, "MainClick");
    confirmPort(); // Η υπάρχουσα συνάρτησή σου για σύνδεση

    return isSerialConnected();
}

bool BackendHandler::isSerialConnected() const {
    return (serial_port && serial_port->isOpen());
}

void BackendHandler::serial_sendCommand(QString command) {
    if (!serial_port || !serial_port->isOpen()) {
        qDebug() << "Serial not open!";
        return;
    }

    if (!command.endsWith("\n"))
        command += "\n";

    QByteArray data = command.toUtf8();
    qint64 bytes = serial_port->write(data);
    serial_port->flush();

    qDebug() << "TX:" << data << "bytes:" << bytes;
}