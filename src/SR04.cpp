#include "SR04.hpp"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <sys/timerfd.h>
#include <unistd.h>

SR04::SR04(const GPIO_config &trigger, const GPIO_config &echo) {
    gpiod::chip chip(trigger.chipName);
    trigger_line_ = chip.get_line(trigger.lineNum);
    if (trigger_line_.is_used()) {
        throw std::runtime_error("Line " + std::to_string(trigger.lineNum) + " is already in use");
    }
    trigger_line_.request(trigger.lineRequest);
    trigger_line_.set_value(0);

    chip = gpiod::chip(echo.chipName);
    echo_line_ = chip.get_line(echo.lineNum);
    if (echo_line_.is_used()) {
        throw std::runtime_error("Line " + std::to_string(echo.lineNum) + " is already in use");
    }
    echo_line_.request(echo.lineRequest);
}

SR04::~SR04() {
    // destructor should never throw exceptions
    try {
        trigger_line_.release();
        echo_line_.release();
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in the destructor: " << e.what() << std::endl;
    }
}

// this works but it is not elegant & didactic
// double SR04::measureDistance() {
//     int duration_ms = 23; // 23ms, max reliable duration for the pulse
//     trigger_line_.set_value(0);
//     usleep(20) ; // 20 microseconds
//     // Wait for the echo line to be low
//     auto echo_ready_time = std::chrono::steady_clock::now();
//     auto echo_ready_timeout = echo_ready_time + std::chrono::milliseconds(duration_ms/2);
//     while (echo_line_.get_value() == 1 && echo_ready_time < echo_ready_timeout) { // wait for the echo line to be low
//         std::this_thread::sleep_for(std::chrono::milliseconds(1));
//         echo_ready_time = std::chrono::steady_clock::now();
//     }
//     if (echo_line_.get_value() == 1) { // echo line should be low before starting the measurement
//         return -1;
//     }
//     // Trigger the ultrasonic burst
//     trigger_line_.set_value(1);
//     usleep(20) ; // 20 microseconds
//     trigger_line_.set_value(0); //prepare for the next measurement
    
//     // Wait for the echo line to go high
//     auto trigger = std::chrono::steady_clock::now();
//     auto now = trigger ;
//     auto timeout_start(trigger + std::chrono::milliseconds(5)); 
//     while (echo_line_.get_value() == 0 && now < timeout_start) {
//         usleep(1);
//         now = std::chrono::steady_clock::now();
//     }
//     auto echo_start = now ;
//     if (echo_line_.get_value() == 0) {
//         return -2;
//     }
//     int duration_us = 0;
//     int timeout_us = 25000 ; // spec says that measurement timeout is 23200 microseconds (4m)
//     while (echo_line_.get_value() == 1 && duration_us < timeout_us ) {
//         usleep(1);
//         duration_us++;
//     }
//     auto echo_end = std::chrono::steady_clock::now();
//     duration_us = std::chrono::duration_cast<std::chrono::microseconds>(echo_end - echo_start).count();
//     if (duration_us > timeout_us) {
//         return 400; // 4m
//     }
//     if ( echo_line_.get_value() == 0) {
//         double duration_seconds = (1.0*duration_us) / 1'000'000.0;
//         double speed_of_sound = 343.0; // m/s
//         // Calculate the total distance traveled by the sound wave
//         auto distance_m = (duration_seconds * speed_of_sound) / 2.0 ; // round trip
//         return distance_m*100.0; // convert to cm
//     } 
//     // sensor error, echo line did not go low within specified time
//     return -3 ;
// }

double SR04::measureDistance() {
    int max_measurement_duration_ms = 25; // 23ms, max reliable duration for the pulse
    // Wait for the echo line to be low
    auto echo_ready_time = std::chrono::steady_clock::now();
    
    auto rising_time = std::chrono::steady_clock::time_point::min();
    auto falling_time = std::chrono::steady_clock::time_point::min();
    auto distance_calculator = [](auto falling_time, auto rising_time) -> float {
        static constexpr float SOUND_SPEED = 340.0f; // Speed of sound in m/s
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(falling_time - rising_time).count();
        // Calculate distance: time[s] * speed[m/s] * 100[cm/m] / 2 (round trip)
        return (duration_us * 1e-6f) * SOUND_SPEED * 100.0f / 2.0f;
    };

    auto echo_ready_timeout = echo_ready_time + std::chrono::milliseconds(max_measurement_duration_ms);
    while (echo_line_.get_value() == 1 && echo_ready_time < echo_ready_timeout) { // wait for the echo line to be low
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        echo_ready_time = std::chrono::steady_clock::now();
    }
    if (echo_line_.get_value() == 1) { // echo line should be low before starting the measurement
        return -1;
    }
    trigger_line_.set_value(1);
    usleep(20) ; // 20 microseconds
    trigger_line_.set_value(0);
    // Wait for the echo line to go high, timeout after 5ms
    auto timeout = std::chrono::milliseconds(5);
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        // Calculate remaining time
        auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        auto remaining_time = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time);
        if (remaining_time <= std::chrono::milliseconds(0)) {
            return -1; // Timeout, no rising edge detected
        }
        // Wait for the event
        auto event = echo_line_.event_wait(remaining_time);
        if (event) {
            auto evt = echo_line_.event_read();
            if (evt.event_type == gpiod::line_event::RISING_EDGE) {
                // Check if the line value is high
                if (echo_line_.get_value() == 1) {
                    rising_time = std::chrono::steady_clock::now();
                    break ; // Rising edge detected and value is high
                }
            }
        }
    }
    // Wait for the echo line to go low, timeout after 230ms
    timeout = std::chrono::milliseconds(230);
    start_time = std::chrono::steady_clock::now();
    while (true) {
        // Calculate remaining time
        auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        auto remaining_time = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time);
        if (remaining_time <= std::chrono::milliseconds(0)) {
            return -2; // Timeout, no falling edge detected
        }
        // Wait for the event
        auto event = echo_line_.event_wait(remaining_time);
        if (event) {
            auto evt = echo_line_.event_read();
            if (evt.event_type == gpiod::line_event::FALLING_EDGE) {
                // Check if the line value is high
                if (echo_line_.get_value() == 0) {
                    falling_time = std::chrono::steady_clock::now();
                    break ; // Rising edge detected and value is high
                }
            }
        }
    }
    return distance_calculator(falling_time, rising_time);
}
