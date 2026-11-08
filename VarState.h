#pragma once
#ifndef VARSTATE_H
#define VARSTATE_H

#include "constants.h"
#include "mqttclient.h"

struct TelemetryData {
    double thrust = 0.0;
    double rpm = 0.0;
    double current = 0.0;
    double voltage = 0.0;
    int throttle = MIN_THROTTLE;
    double power = 0.0;
    double torque = 0.0;
};

struct ControlSurfaces {
    int channel;
    int low_pwm_value;
    int high_pwm_value;
    int current_value;
    double min_angle;
    double max_angle;
    double current_angle;
};

struct CalibrationConfig {
    const float current_lsb_user_esp = 0.00003688f;
    float current_lsb_user_selected = 0.00003688f;
    bool manual_current_calibration = false;

    const float esp_actual_tare_offset = 38468.909f;
    const float esp_thrust_calibration_factor = 0.4485f;
    float direction_sign = -1.0f;
    float thrust_calibration_factor = 0.4485f;
    float tare_newton_offset = 38468.909f;
    bool manual_thrust_calibration = false;
    bool auto_tare_thrust_at_begin = false;

    double thrust_offset = 0.0;
    double current_offset = 0.0;

    float gravity = 9.81f;

    int min_throttle = MIN_THROTTLE;
    int max_throttle = MAX_THROTTLE;
};

struct MqttConfig {
    QString mqtt_receive_topic = DEFAULT_MQTT_RECEIVE_TOPIC;
    QString mqtt_send_topic = DEFAULT_MQTT_SEND_TOPIC;
    int mqtt_port = DEFAULT_MQTT_PORT;
    MqttClient* mqtt_client = nullptr;
    QString mqtt_broker = DEFAULT_MQTT_BROKER;
    QByteArray last_mqtt_data;
};

#endif