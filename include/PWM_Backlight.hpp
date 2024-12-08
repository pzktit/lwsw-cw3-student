#pragma once

#include "Hardware_PWM.hpp"
#include <system_error>

class PWM_Backlight : public Hardware_PWM {
public:
    struct Config {
        int pwmChip;
        int pwmChannel;
    };
    PWM_Backlight(const Config &config)
        : Hardware_PWM(config.pwmChip, config.pwmChannel, 200*usecs, 100*usecs) {} //

    void setBrightness(int brightness_percent) {
        if (brightness_percent < 0 || brightness_percent > 100) {
            throw std::runtime_error("Brightness must be between 0 and 100 percent");
        }

        unsigned long int duty_cycle = static_cast<unsigned long int>(period_ * (static_cast<double>(brightness_percent) / 100.0));
        duty_cycle = std::min(duty_cycle, period_);
        setDuty(duty_cycle);
    }
};
