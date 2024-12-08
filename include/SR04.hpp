#pragma once

#include "GPIO_config.hpp"
#include <gpiod.hpp>
#include <string>

class SR04 {
public:
    SR04(const GPIO_config &trigger, const GPIO_config &echo);
    ~SR04();
    double measureDistance();

private:
    gpiod::line trigger_line_;
    gpiod::line echo_line_;
};
