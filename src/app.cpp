/* ***************************************************************** 

Exercise 3: Managing GPIO inputs: push buttons, rotary encoder, and SR04 ultrasonic sensor
by Piotr ZAWADZKI, Copyright (c) 2025

Application Features:

- The SR-04 rangefinder continuously measures the distance to an obstacle,
- When the obstacle is too close, an alarm procedure is initiated,
- The detection threshold value is set using a rotary encoder knob,
- An active alarm can be deactivated by pressing the rotary encoder button,
- The measured distance to the obstacle and the set alarm activation threshold are displayed on the screen,
- The distance value that triggered the alarm is displayed in red,
- For testing purposes, the alarm can be triggered by briefly pressing a momentary button,
- A long press of the momentary button ends the application,
- The application can be terminated with the _Ctrl+C_ combination. **Note! _Ctrl+C_ does not work in the Visual Studio Code terminal**.

 **************************************************************** */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <linux/input.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "GPIO_config.hpp"
#include "st7789v2.hpp"
#include "GPIO_Led.hpp"
#include "SR04.hpp"

// Hardware configuration and initialization
const struct Hardware_config {
    const ST7789::Config displayConfig;
    const GPIO_Led::Config LED;
    const std::string buttonEvents;
    const GPIO_config SR04_Echo ;
    const GPIO_config SR04_Trigger;
    const GPIO_config displayBacklightConf;
    const GPIO_config rotary_SIA;
    const GPIO_config rotary_SIB;
    const GPIO_config rotary_SW;
} hardwareConfig = {
    .displayConfig = {
        .spiDevice = "/dev/spidev0.0",
        .speedHz = 30000000,  // 30 MHz, in theory ST7789v2 should support up to 62.5 MHz
        .gpioChip = "gpiochip0",
        .dcPin = 22,
        .resetPin = 27
    }
    ,
    .LED = {"lwsw-led"}
    ,
    .buttonEvents = "/dev/input/event0"
    ,
    .SR04_Echo = {
        .chipName = "gpiochip0",
        .lineNum = 24,
        .lineRequest = {
            .consumer = "SR04-Echo",
            .request_type = gpiod::line_request::EVENT_BOTH_EDGES,
            .flags = gpiod::line_request::FLAG_BIAS_PULL_DOWN
        }
    }
    ,
    .SR04_Trigger = {
        .chipName = "gpiochip0",
        .lineNum = 23,
        .lineRequest = {
            .consumer = "SR04-Trigger",
            .request_type = gpiod::line_request::DIRECTION_OUTPUT,
            .flags = 0
        }
    }
    ,
    .displayBacklightConf = {
        .chipName = "gpiochip0",
        .lineNum = 17,
        .lineRequest = {
            .consumer = "displayBacklight",
            .request_type = gpiod::line_request::DIRECTION_OUTPUT,
            .flags = 0
        }
    }
    ,
    .rotary_SIA = {
        .chipName = "gpiochip0",
        .lineNum = 16,
        .lineRequest = {
            .consumer = "rotary_SIA",
            .request_type = gpiod::line_request::EVENT_BOTH_EDGES,
            .flags = gpiod::line_request::FLAG_BIAS_PULL_DOWN
        }
    }
    ,
    .rotary_SIB = {
        .chipName = "gpiochip0",
        .lineNum = 20,
        .lineRequest = {
            .consumer = "rotary_SIB",
            .request_type = gpiod::line_request::DIRECTION_INPUT,
            .flags = gpiod::line_request::FLAG_BIAS_PULL_DOWN
        }
    }
    ,
    .rotary_SW = {
        .chipName = "gpiochip0",
        .lineNum = 21,
        .lineRequest = {
            .consumer = "rotary_SW",
            .request_type = gpiod::line_request::EVENT_BOTH_EDGES,
            .flags = gpiod::line_request::FLAG_BIAS_PULL_DOWN
        }
    }
};

// Global variable for synchronization and state sharing
// between the monitoring threads and the main thread
// see "custom_types.hpp" for type definition 

const auto noalarmTime = std::chrono::steady_clock::time_point::min();
const unsigned int alarmDuration_ms = 20000;

struct Application_state {
    std::atomic<bool> keepRunning;
    std::atomic<bool> setAlarm;
    std::atomic<std::chrono::steady_clock::time_point> alarmTime;
    std::atomic<float> distance; // in cm, distance measured by the sensor
    std::atomic<int> proximityThreshold; // in cm, distance below which the alarm is triggered
} appState = {
    .keepRunning = true,
    .setAlarm = false,
    .alarmTime = noalarmTime,
    .distance = 400.0,
    .proximityThreshold = 10
};

// Graceful program close on Ctrl+C
void sigint_handler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    appState.keepRunning.store(false);
}

// Prototypes for the threads
void button_thread( Application_state & appState, const std::string & inputDevice) ;
void detection_thread( Application_state & appState, const GPIO_config & SR04_Trigger, const GPIO_config & SR04_Echo) ;
void rotary_encoder_thread(Application_state & appState, const GPIO_config & SIA_config, const GPIO_config & SIB_config) ;
void display_thread( Application_state & appState, const ST7789::Config & displayConfig) ;
void alarm_thread( Application_state & appState, unsigned int duration_ms) ;
void rotary_button_thread( Application_state & appState, const GPIO_config & SW_config) ;
void gpio_led_thread(Application_state & appState, const GPIO_Led::Config & led_config) ;
void backlight_led_thread(Application_state & appState, const GPIO_config & displayBacklight_config) ;

int main() {
    try {
        signal(SIGINT, sigint_handler); // Register signal handler for Ctrl+C
        std::thread lwsw_button_task( button_thread, std::ref(appState), hardwareConfig.buttonEvents) ;
        std::thread detection_task( detection_thread, std::ref(appState), hardwareConfig.SR04_Trigger, hardwareConfig.SR04_Echo) ;
        std::thread rotary_task( rotary_encoder_thread, std::ref(appState), hardwareConfig.rotary_SIA, hardwareConfig.rotary_SIB) ;
        std::thread display_task( display_thread, std::ref(appState), hardwareConfig.displayConfig) ;
        std::thread rotary_button_task( rotary_button_thread, std::ref(appState), hardwareConfig.rotary_SW) ;
        std::thread alarm_led_task( gpio_led_thread, std::ref(appState), hardwareConfig.LED ) ;
        std::thread displayBacklight_task( backlight_led_thread, std::ref(appState), hardwareConfig.displayBacklightConf) ;

        while (appState.keepRunning.load()) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto now = std::chrono::steady_clock::now();
            if (appState.setAlarm.load() && now > appState.alarmTime.load()) { 
                std::cout << "ALARM expired" << std::endl;
                appState.setAlarm.store(false);
                appState.alarmTime.store(std::chrono::steady_clock::time_point::min());
            }
        }

        std::cout << "Main thread: waiting for child threads stop." << std::endl;
        displayBacklight_task.join();
        alarm_led_task.join();
        rotary_button_task.join();
        display_task.join();
        rotary_task.join();
        detection_task.join();
        lwsw_button_task.join() ;
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in main thread: " << e.what() << std::endl;
    }
    std::cout << "Application gracefully stopped." << std::endl;
    return 0;
}

void handle_gpio_key_event(const input_event &input_event, Application_state &appState) {
    static std::chrono::steady_clock::time_point press_time = std::chrono::steady_clock::time_point::min();
    static bool button_pressed = false;

    if (input_event.value == 1 && !button_pressed) { // Press
        press_time = std::chrono::steady_clock::now();
        button_pressed = true;
    } else if (input_event.value == 0 && button_pressed) { // Release
        auto press_duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - press_time).count();
        if (press_duration > 500) { // Press longer than 500ms
            button_pressed = false;
            std::cout << "Application exit" << std::endl;
            appState.keepRunning.store(false);            
        } else {
            // TODO: Dodaj obsługę krótkiego naciśnięcia przycisku polegającą na aktywacji stanu alarmu
            // Wskazówka: Podobne działanie znajduje się w kodzie wątku obsługującego dalmierz
        }
    }
}

void button_thread(Application_state &appState, const std::string &inputDevice) {
    try {
        int epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            throw std::runtime_error("Failed to create epoll instance");
        }

        int fd = open(inputDevice.c_str(), O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("Failed to open device: " + inputDevice);
        }

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            close(fd);
            throw std::runtime_error("Failed to add file descriptor to epoll: " + inputDevice);
        }

        std::cout << __func__ << " started for device: " << inputDevice << std::endl;

        struct epoll_event event;
        struct input_event input_event;

        while (appState.keepRunning.load()) {
            // Wait for single event with 10ms timeout
            int ready = epoll_wait(epoll_fd, &event, 1, 10);
            
            if (ready < 0) {
                throw std::runtime_error("Epoll wait error occurred");
            }
            
            if (ready > 0 && (event.events & EPOLLIN)) {
                ssize_t bytes = read(event.data.fd, &input_event, sizeof(struct input_event));
                
                if (bytes == sizeof(struct input_event) && input_event.type == EV_KEY) {
                    handle_gpio_key_event(input_event, appState);
                }
            }
        }        
        // this is C API, always clean up after yourself
        close(fd);
        close(epoll_fd);
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in Button monitoring thread: " << e.what() << " in " << __func__ << std::endl;
    }
    std::cout << __func__ << " thread finished." << std::endl;
}

void rotary_button_thread( Application_state & appState, const GPIO_config& SW_config) {
    try {
        gpiod::chip chip(SW_config.chipName);
        gpiod::line SW_line = chip.get_line(SW_config.lineNum);
        SW_line.request(SW_config.lineRequest);

        std::cout << "Monitoring Rotary button" << std::endl;

        std::cout << __func__ << " started." << std::endl;
        while ( appState.keepRunning.load() ) {
            auto event = SW_line.event_wait(std::chrono::milliseconds(100));
            if (event) {
                auto SW_event = SW_line.event_read();
                if (SW_event.event_type == gpiod::line_event::RISING_EDGE) { // button pressed, see the hardware configuration
                    //TODO: Dodaj obsługę naciśnięcia przycisku polegającą na zresetowaniu stanu alarmu
                } else { // button released
                    // std::cout << "Button released" << std::endl;
                }
            }
        }
        SW_line.release();
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in rotary button monitoring thread: " << e.what() << std::endl;
    }
    std::cout << __func__ << " thread finished." << std::endl;
}

void rotary_encoder_thread(Application_state & appState, const GPIO_config& SIA_config, const GPIO_config& SIB_config) {
    try {
        const int proximityThresholdDelta = 1 ;
        const int proximityThresholdMin = 0 ;
        const int proximityThresholdMax = 98 ;
        // full quadrature decoding
        //TODO: Uzupełnij funkcję dekodującą enkoder rotacyjny
        // W aplikacji `demo` enkoder zlicza co 2 impulsy, ze względu na to, że w zastosowanym enkoderze co druga pozycja jest stabilna
        // Dostosuj poniższy kod tak aby enkoder zliczał co 1 impuls. 
        auto rotary_decoder = [](int clk, int dt) -> int {
                if (clk == 1 && dt == 0) {
                    return 0; //TODO: Dodaj właściwą wartość dekodowania (+1 lub -1 lub 0) pozycji enkodera // CW on rising edge of CLK with DT=0
                } else if (clk == -1 && dt == 1) {
                    return 0; //TODO: Dodaj właściwą wartość dekodowania (+1 lub -1 lub 0) pozycji enkodera // CW on falling edge of CLK with DT=1
                } else if (clk == 1 && dt == 1) {
                    return 0; //TODO: Dodaj właściwą wartość dekodowania (+1 lub -1 lub 0) pozycji enkodera // CCW on rising edge of CLK with DT=1
                } else if (clk == -1 && dt == 0) {
                    return 0; //TODO: Dodaj właściwą wartość dekodowania (+1 lub -1 lub 0) pozycji enkodera // CCW on falling edge of CLK with DT=0
                }
                return 0; // No rotation
            };
        // get lines
        gpiod::chip SIA_chip(SIA_config.chipName);
        gpiod::line SIA_line = SIA_chip.get_line(SIA_config.lineNum);
        SIA_line.request(SIA_config.lineRequest);

        gpiod::chip chip(SIB_config.chipName);
        gpiod::line SIB_line = chip.get_line(SIB_config.lineNum);
        SIB_line.request(SIB_config.lineRequest);

        std::cout << __func__ << " started." << std::endl;
        while ( appState.keepRunning.load() ) {
            // Wait for an edge event
            auto event = SIA_line.event_wait(std::chrono::milliseconds(100));
            if (event) {
                auto SIA_event = SIA_line.event_read();
                int SIA_Value = SIA_line.get_value();
                int SIB_Value = SIB_line.get_value();
                // if true rising edge (i.e. edge and correct value) or falling edge
                if ( (SIA_event.event_type == gpiod::line_event::RISING_EDGE && SIA_Value == 1) || (SIA_event.event_type == gpiod::line_event::FALLING_EDGE && SIA_Value == 0) ) { 
                    int rotation = rotary_decoder(
                        SIA_event.event_type == gpiod::line_event::RISING_EDGE ? 1 : -1, SIB_Value);
                    int newThreshold = appState.proximityThreshold.load() ;
                    newThreshold += rotation * proximityThresholdDelta ; // increase or decrease the threshold
                    newThreshold = std::max(proximityThresholdMin, std::min(proximityThresholdMax, newThreshold)) ; // constraint threshold to predefined limits
                    appState.proximityThreshold.store(newThreshold) ;
                }
            }
        }
        // this is not required because of RAII
        // SIA_line.release();
        // SIB_line.release();
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in GPIO monitoring thread: " << e.what() << std::endl;
    }
    std::cout << __func__ << " thread finished." << std::endl;
}
void detection_thread(Application_state & appState, const GPIO_config & SR04_Trigger, const GPIO_config & SR04_Echo) {
    try {
        SR04 sr04(SR04_Trigger, SR04_Echo);
        // ignore the first few readings
        for (size_t i = 0; i < 5; i++) {
            sr04.measureDistance(); 
        }
        // sart detection
        std::cout << __func__ << " started." << std::endl;
        while (appState.keepRunning.load()) {
            if (!appState.setAlarm.load()) {
                auto distance = sr04.measureDistance();
                if (distance < 0) {
                    std::cout << "Error reading distance. Err code:" << distance << std::endl;
                    // wait for system to stabilize
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                } else {
                    // std::cout << "Distance: " << distance << " cm" << std::endl;
                    appState.distance.store(distance);
                    if (appState.distance.load() < appState.proximityThreshold.load()) {
                        std::cout << "ALARM! Distance " << appState.distance.load() << "cm below threshold" << std::endl;
                        appState.setAlarm.store(true);
                        appState.alarmTime.store(std::chrono::steady_clock::now() +  std::chrono::milliseconds(alarmDuration_ms)); 
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in detection thread: " << e.what() << std::endl;
    }
    std::cout << __func__ << " thread finished." << std::endl;
}

void display_thread( Application_state & appState, const ST7789::Config & displayConfig) {
    try {
        ST7789 display(displayConfig);
        display.clearScreen(ST7789::Colors::BLACK);
        display.showLogo();
        std::cout << __func__ << " started." << std::endl;
        while (appState.keepRunning.load()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << std::setw(6) << std::setfill(' ') << appState.distance.load() << " cm";
            if (appState.setAlarm.load()) {
                display.drawString(0, 16, oss.str(), ST7789::Colors::RED, ST7789::Colors::BLACK);
            } else {
                display.drawString(0, 16, oss.str(), ST7789::Colors::WHITE, ST7789::Colors::BLACK);
            }
            // std::cout << "Dist: " << appState.distance.load() << " cm\t" ; 
            oss.str("");
            oss << std::setw(2) << std::setfill(' ') << appState.proximityThreshold.load() << " cm" ;
            // std::cout << "Threshold: " << oss.str() << std::endl;
            display.drawString(240-(2+3)*16, 16, oss.str(), ST7789::Colors::WHITE, ST7789::Colors::BLACK);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        display.clearScreen( ST7789::Colors::BLACK );
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in display thread: " << e.what() << std::endl;
    }
    std::cout << __func__ << " thread finished." << std::endl;
}

void backlight_led_thread(Application_state & appState, const GPIO_config & displayBacklight_config) {
    try {
        gpiod::chip chip(displayBacklight_config.chipName);
        gpiod::line displayBacklight = chip.get_line(displayBacklight_config.lineNum);
        displayBacklight.request(displayBacklight_config.lineRequest);
        displayBacklight.set_value(1);

        std::cout << __func__ << " started." << std::endl;
        while (appState.keepRunning.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        displayBacklight.set_value(0);
        displayBacklight.release();
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in backlight led thread: " << e.what() << std::endl;
    }
    std::cout << __func__ << " thread finished." << std::endl;
}

void gpio_led_thread(Application_state & appState, const GPIO_Led::Config & led_config) {
    try {
        GPIO_Led led(led_config);
        led.setTrigger("default-on");
        led.set(GPIO_Led::ON);
        static const auto loop_time_step = std::chrono::milliseconds(100); 
        std::cout << __func__ << " started." << std::endl;
        while (appState.keepRunning.load()) {
            auto now=std::chrono::steady_clock::now();
            if (now < appState.alarmTime.load()) {
                // set the alarm state
                led.setTrigger("heartbeat");
                // wait for the alarm to expire
                while (appState.keepRunning.load() && now < appState.alarmTime.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    now = std::chrono::steady_clock::now();
                }
                led.setTrigger("default-on");
            }
            std::this_thread::sleep_for(loop_time_step);
        }
        led.setTrigger("none");
        led.set(GPIO_Led::OFF) ;
    } catch (const std::exception &e) {
        std::cerr << "An error occurred in LED thread: " << e.what() << std::endl;
    }
    std::cout << __func__ << " thread finished." << std::endl;
}
