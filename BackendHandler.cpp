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
#include "mqttclient.h"
#include "containers.h"
#include "graphcontainer.h"
#include <QFrame>
#include <QtSerialPort/QSerialPortInfo>
#include "VarState.h"

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



double BackendHandler::RPMFilter(double new_rpm) {
    rpm_buffer.push_back(new_rpm);

    if (rpm_buffer.size() > rpm_filter_window) {
        rpm_buffer.erase(rpm_buffer.begin());
    }

    QVector<double> sorted_buffer = rpm_buffer;
    std::sort(sorted_buffer.begin(), sorted_buffer.end());

    return sorted_buffer[sorted_buffer.size() / 2];
}

// Telemetry Functions
void BackendHandler::receiveTelemetry(const QString& data, QString source) {

    if (source != selected_data_source) return;

    if (selected_data_source == "MQTT") {
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
        updateTelemetry(value_keeper);
    }
    else {
        QString cleanData = data.trimmed();
        cleanData.replace("V = ", "V:");
        cleanData.replace("I = ", "I:");
        cleanData.replace(" V,", ",");
        cleanData.replace(" A,", ",");
        cleanData.replace(" ", "");

        QMap<QString, double> value_keeper;
        QStringList parts = cleanData.split(",");

        for (const QString& part : parts) {
            QStringList pair = part.split(":");
            if (pair.size() >= 2) {
                bool ok;
                double val = QLocale::c().toDouble(pair[1], &ok);
                if (ok) {
                    value_keeper[pair[0]] = val;
                }
            }
        }

        if (!value_keeper.isEmpty()) {
            updateTelemetry(value_keeper);
        }
    }

    bool previous_write_status = write_logs;

    if (tel.voltage == battery_removed_voltage_value) tel.voltage = 0;

    if (tel.voltage == 0) {
        write_logs = false;
        if (is_armed) emit armedStateChanged(false); // disarm
    }
    else {
        if (!is_armed) emit armedStateChanged(true); // an den einai armed kanto gia tash!=0
    }

    if (write_logs != previous_write_status) {
        emit loggingStateChanged(write_logs);
    }

    logToFile(true);
}

void BackendHandler::updateTelemetry(QMap<QString, double> value_keeper) {

    tel.rpm = value_keeper.value("RPM", tel.rpm);
    tel.current = value_keeper.value("Current", tel.current);
    tel.voltage = value_keeper.value("Voltage", tel.voltage);
    tel.throttle = value_keeper.value("Throttle", tel.throttle);
    tel.power = tel.voltage * tel.current;

    if (use_rpm_filter) {
        tel.rpm = RPMFilter(tel.rpm);
    }
    else {
        RPMFilter(tel.rpm);
    }

    emit telemetryUpdated();
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

    int label_width = 26;
    QFile file(log_file_path);
    bool write_header = (!file.exists() || file.size() == 0);

    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Error opening log file:" << file.errorString();
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    double throttle_pct = getThrottlePercentage(tel.throttle);

    // ΔΥΝΑΜΙΚΗ ΛΙΣΤΑ LOGGING: Προσθέτεις/αφαιρείς ό,τι θες εδώ σε 1 γραμμή!
    QVector<QPair<QString, QString>> log_columns = {
        { "Timestamp",                 timestamp },
        { "Thrust (N)",                QString::number(tel.thrust, 'f', 2) },
        { "Voltage (V)",               QString::number(tel.voltage, 'f', 2) },
        { "Current (A)",               QString::number(tel.current, 'f', 2) },
        { "RPM",                       QString::number(tel.rpm, 'f', 0) },
        { "Throttle (%)",              QString::number(throttle_pct, 'f', 2) },
        { "Power (W)",                 QString::number(tel.power, 'f', 2) }
    };

    QTextStream out(&file);

    // Αν το αρχείο είναι άδειο, γράφει αυτόματα τις επικεφαλίδες από τη λίστα!
    if (write_header) {
        for (const auto& col : log_columns) {
            out << QString("%1 ").arg(col.first, label_width);
        }
        out << "\n";
    }

    // Γράφει τις τιμές
    for (const auto& col : log_columns) {
        out << QString("%1 ").arg(col.second, label_width);
    }
    out << "\n";

    file.close();
}

void BackendHandler::handleSerial(bool status) {
    if (status) {
        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Closed previous serial port.";
        }

        serial_port->setPortName(serial_com_port_tag);
        serial_port->setBaudRate(serial_baudrate);

        if (serial_port->open(QIODevice::ReadWrite)) {
            connect(serial_port, &QSerialPort::readyRead, this, &BackendHandler::readSerialData, Qt::UniqueConnection);
            qDebug() << "Opened serial port" << serial_com_port_tag << "at" << serial_baudrate;
        }
        else {
            qWarning() << "Cannot open port" << serial_com_port_tag << ":" << serial_port->errorString();
        }
    }
    else {
        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Serial port closed.";
        }
    }
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

void BackendHandler::confirmPort() {
    qDebug() << "Selected COM port:" << serial_com_port_tag;
    qDebug() << "Selected baudrate:" << serial_baudrate;
    handleSerial(running);
}

bool BackendHandler::isSerialConnected() const {
    return (serial_port && serial_port->isOpen());
}

void BackendHandler::readSerialData() {
    while (serial_port->canReadLine()) {
        QByteArray data = serial_port->readLine();
        if (!data.isEmpty()) {
            QString serialData = QString::fromUtf8(data).trimmed();
            qDebug() << "Full Line Received:" << serialData;
            receiveTelemetry(serialData, "SERIAL");
        }
    }
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
// ------------------------------------------------------------------------
double BackendHandler::getThrottlePercentage(int current_throttle) {

    if (cal.max_throttle <= cal.min_throttle)
        return 0.0;

    return 100.0 * (current_throttle - cal.min_throttle) / (cal.max_throttle - cal.min_throttle);
}

