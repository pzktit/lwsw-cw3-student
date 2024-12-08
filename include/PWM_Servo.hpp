#pragma once

#include "Hardware_PWM.hpp"
#include <cmath>
#include <system_error>

class PWM_Servo : public Hardware_PWM {
public:
    struct Config {
        int pwmChip;
        int pwmChannel;
        float minAngle;
        float maxAngle;
    };

    PWM_Servo(const Config &config)
        : Hardware_PWM(config.pwmChip, config.pwmChannel, 20*msecs, 1500*usecs)
        , min_angle_(config.minAngle)
        , max_angle_(config.maxAngle) 
        , angle_(0.0) 
        {}

    const unsigned long int maxCCW = 1*msecs;
    const unsigned long int maxCW = 2*msecs;

    void setAngle(float angle) {
        if (angle < min_angle_ || angle > max_angle_) {
            throw std::runtime_error("Angle must be between " + std::to_string(min_angle_) + " and " + std::to_string(max_angle_));
        }
        angle_ = angle;
        setDuty(getDutyFromAngle());
    }

protected:
    virtual unsigned long int getDutyFromAngle() {
        // Map the angle to the duty cycle
        // angle = min_angle_ -> duty = maxCCW
        // angle = max_angle_ -> duty = maxCW
        // angle = 0 -> duty = 1.5ms (neutral position, symmetric setup)
        return static_cast<unsigned long int>(maxCCW + (angle_ - min_angle_) / (max_angle_-min_angle_) * (maxCW - maxCCW));
    }

    float min_angle_;
    float max_angle_;
    float angle_;
};
