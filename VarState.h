#pragma once
#ifndef VARSTATE_H
#define VARSTATE_H

#include "constants.h"
#include "mqttclient.h"

struct TelemetryData {
    double main_batt_voltage = 0.0;
    double main_batt_current = 0.0;
    double servo_batt_voltage = 0.0;
    double servo_batt_current = 0.0;
    double ground_speed = 0.0;
    double altitude = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;
    double homedist = 0.0;
    int sat_count = 0;
    double pi_temp = 0.0;
    int current_throttle = MIN_THROTTLE;
    double current_power = 0.0;
    
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

struct PlaneControlSurfaces {
    ControlSurfaces left_aileron{ 1, 988, 2012, 0, -45.0, 45.0, 0.0 };
    ControlSurfaces right_aileron{ 1, 988, 2012, 0, -45.0, 45.0, 0.0 };
    ControlSurfaces elevator{ 2, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces motor{ 3, 988, 2012, 0 , -45.0, 45.0, 0.0 };
    ControlSurfaces rudder{ 4, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces left_flap{ 5, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces right_flap{ 5, 988, 2012, 0 , -45.0, 45.0 , 0.0 };
    ControlSurfaces landing_gear1{ 7, 988, 2012, 0 , 0.0, 90.0 , 0.0 };
    ControlSurfaces landing_gear2{ 8, 988, 2012, 0 , 0.0, 90.0 , 0.0 };
    ControlSurfaces arm_status_switch{ 9, 988, 2012,0 };
};


struct CalibrationConfig {

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

double mapValue(double x, double in_min, double in_max, double out_min, double out_max);

#endif