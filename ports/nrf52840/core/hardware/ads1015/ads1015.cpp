#include <map>




#include "bsp.hpp"
#include "nrf_i2c.hpp"
#include "error.hpp"
#include "pmu.hpp"
#include "debug.hpp"

#include "ads1015.hpp"
#include "gpio.hpp"



static int flag = 1;



ads1015LL::ads1015LL(unsigned int bus, unsigned char addr, int wakeup_pin) : m_bus(bus), m_addr(addr), m_wakeup_pin(wakeup_pin) {
    

}

void ads1015LL::send_command(uint8_t command)
{
  DEBUG_TRACE("ads1015LL::send_command");
	NrfI2C::write(m_bus, m_addr, (uint8_t *)&command, sizeof(command), false);
}

void ads1015LL::send_command_conf(uint8_t *command)
{
  DEBUG_TRACE("ads1015LL::send_command_conf");
	NrfI2C::write(m_bus, m_addr, command, 3, false);
}


void ads1015LL::read(int& digital_value) {
    //start trigger
    //code gpio trig ;
    //int flag = 1;

    //DEBUG_TRACE("BAROMETER_SLEEP_EN is set to HIGH");
     //GPIOPins::set(BAROMETER_SLEEP_EN);
     

    DEBUG_TRACE("Flag value before if statement: %d", flag);
    if (flag==1)
    {
      flag=0;
      uint8_t write_buffer[3] = {ads1015Command::CONFIG_REG,ads1015Command::MSB_CONFIG,ads1015Command::LSB_CONFIG};
      send_command_conf(write_buffer);
      PMU::delay_ms(10);
      flag = 0;  
    }
    DEBUG_TRACE("sample adc test0");
    uint16_t bin_value = sample_adc(ads1015Command::CONV_REG);
    DEBUG_TRACE("sample adc test");

    DEBUG_TRACE("ads1015LL::read: %u bin", bin_value); 
    bin_value = bin_value >> 5; // /!!! Tom test ">> 5" normaly  
    digital_value = (int)(bin_value);  
    DEBUG_TRACE("ads1015LL::read: digital value = %u ", digital_value);
    DEBUG_TRACE("ads1015LL::read: %u bin >>5 digital value = %u ", bin_value,digital_value); 
    //digital_value = (double)digital_value; 
    DEBUG_TRACE("ads1015LL::read: %u digit", digital_value);
    digital_value = (digital_value/2047)*5;
    DEBUG_TRACE("ads1015LL::read: %u Volt", digital_value);
    digital_value = 800 +((1060-800)/5)*digital_value;
    DEBUG_TRACE("ads1015LL::read: %u hPa", digital_value);
}



uint16_t ads1015LL::sample_adc(uint8_t measurement) {
    DEBUG_TRACE("ads1015LL::sample_adc0");
    uint8_t cmd = measurement;  
    send_command(cmd);
    PMU::delay_ms(10);

    uint8_t read_buffer[2];
    DEBUG_TRACE("ads1015LL::sample_adc1");
    NrfI2C::read(m_bus,m_addr,read_buffer,2);
    DEBUG_TRACE("ads1015LL::sample_adc2 ");
    return ((uint16_t)read_buffer[0] << 8 ) + ((uint16_t)read_buffer[1]);
    
}

ads1015::ads1015() : Sensor("BARO"), m_ads1015(ads1015LL(ADC_BAROMETER_DEVICE,ADC_BAROMETER_ADDR,BAROMETER_SLEEP_EN)),m_digital_value(0) {
DEBUG_TRACE("ads1015::ads1015");
}

double ads1015::read(unsigned int offset = 0) {
    
		if (0 == offset) {
			m_ads1015.read(m_digital_value);
			return m_digital_value;
		}
		throw ErrorCode::BAD_SENSOR_CHANNEL;
		
	}
