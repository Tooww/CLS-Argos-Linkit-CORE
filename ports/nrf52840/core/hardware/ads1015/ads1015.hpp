#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include "sensor.hpp"
#include "bsp.hpp"
#include "error.hpp"

class ads1015LL {
public:
	unsigned int m_bus;
	unsigned char m_addr;
	int m_wakeup_pin;
	ads1015LL(unsigned int bus, unsigned char addr, int wakeup_pin);
	//~ads1015LL();
	void read(double& digital_value);
	
    //void set_wakeup_threshold(double threshold);
	//void set_wakeup_duration(double duration);
	//void enable_wakeup(std::function<void()> func);
	//void disable_wakeup();
	//bool check_and_clear_wakeup();

private:

    enum ads1015Command : uint8_t {
		CONV_REG = (0x00),
		CONFIG_REG = (0x01),
		MSB_CONFIG = (0X81), // for config FSR=6.144V ref datasheet ads1015
		LSB_CONFIG =(0x83), // for config FSR=6.144V (defaul lsb)
	};

	
	
	void send_command(uint8_t command);
	void send_command_conf(uint8_t *command);
	uint16_t sample_adc(uint8_t measurement);
	//NrfIRQ m_irq;
	//uint8_t m_unique_id;
	//uint8_t m_accel_sleep_mode;
	//struct bmx160_dev m_bmx160_dev;
	//double m_wakeup_threshold;
	//double m_wakeup_slope;
	//double m_wakeup_duration;
	//bool m_irq_pending;

	//void init();
	
};



class ads1015 : public Sensor {
public:
    ads1015(); 
	double read(unsigned int offset) override; 

private:
ads1015LL m_ads1015;
int m_digital_value; 
};