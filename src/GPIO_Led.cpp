#include "GPIO_Led.hpp"

#include <iostream>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>

GPIO_Led::GPIO_Led(const Config& config) : led_name_(config.led_name), fd_(-1), last_value_(OFF) {
    try {
        std::string brightness_path = sysdir + led_name_ + "/brightness";
        fd_ = open(brightness_path.c_str(), O_WRONLY);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to open brightness file for LED: " + led_name_);
        }
        setTrigger("default-on");
        set(ON);
    } catch (const std::exception& e) {
        cleanup();
        throw;
    }
}

GPIO_Led::~GPIO_Led() {
    try {
        if (fd_ >= 0) {
            set(OFF);
            setTrigger("none");
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in destructor: " << e.what() << std::endl;
    }
    cleanup();
}

void GPIO_Led::set(bool value) {
    try {
        std::string value_str = value ? "1" : "0";
        // std::cout << "Writing to " << (sysdir + led_name_ + "/brightness") << ": " << value_str << std::endl;
        if (write(fd_, value_str.c_str(), value_str.size()) < 0) {
            throw std::runtime_error("Failed to write to brightness file for LED");
        }
        last_value_ = value;
    } catch (const std::exception& e) {
        throw;
    }
}

void GPIO_Led::set(int value) {
    set(value != 0);
}

bool GPIO_Led::get() const {
    return last_value_;
}

void GPIO_Led::toggle() {
    set(!last_value_);
}

void GPIO_Led::setTrigger(const std::string& value) {
    int trigger_fd = -1;
    try {
        std::string trigger_path = sysdir + led_name_ + "/trigger";
        trigger_fd = open(trigger_path.c_str(), O_WRONLY);
        if (trigger_fd < 0) {
            throw std::runtime_error("Failed to open trigger file for LED: " + led_name_);
        }
        if (write(trigger_fd, value.c_str(), value.size()) < 0) {
            throw std::runtime_error("Failed to write to trigger file for LED: " + led_name_);
        }
    } catch (const std::exception& e) {
        if (trigger_fd >= 0) {
            close(trigger_fd);
        }
        throw;
    }
    if (trigger_fd >= 0) {
        close(trigger_fd);
    }
}

void GPIO_Led::cleanup() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}
