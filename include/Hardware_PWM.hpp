#pragma once

#include <string>

class Hardware_PWM {
public:
    Hardware_PWM(int pwm_chip, int pwm_channel, unsigned long int period, unsigned long int duty_cycle);
    ~Hardware_PWM();

    static const unsigned long int secs  = 1000000000;
    static const unsigned long int msecs = 1000000;
    static const unsigned long int usecs = 1000;
    static const unsigned long int nsecs = 1;

    void exportPWM();
    void unexportPWM();
    void setDuty(unsigned long int duty_cycle);
    void setPeriod(unsigned long int period);
    void enable();
    bool isEnabled() ;
    void disable();

protected:
    virtual std::string getPWMChipPath() const;
    virtual std::string getPWMPath(const std::string& file) const;

private:
    std::string readFromFile(const std::string& path) const;
    void writeToFile(const std::string& path, int value);

protected:
    unsigned long int period_;
    unsigned long int duty_cycle_;

private:
    int pwm_chip_;
    int pwm_channel_;
};
