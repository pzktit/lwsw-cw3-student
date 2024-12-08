#pragma once

#include <gpiod.hpp>
#include <string>

// Structure to hold GPIO configuration
struct GPIO_config {
    std::string chipName;
    unsigned int lineNum;
    gpiod::line_request lineRequest;
};