#include <map>




#include "bsp.hpp"
#include "nrf_i2c.hpp"
#include "error.hpp"
#include "pmu.hpp"
#include "debug.hpp"

#include "ads1015.hpp"
#include "gpio.hpp"

#include "../../../bsp/gentracker_v1.0/bsp.hpp"

static int flag = 1;



ads1015LL::ads1015LL(unsigned int bus, unsigned char addr, int wakeup_pin) {
    m_bus = bus;
    m_addr = addr; 


}

void ads1015LL::send_command(uint8_t command)
{
	NrfI2C::write(m_bus, m_addr, (uint8_t *)&command, sizeof(command), false);
}

void ads1015LL::send_command_conf(uint8_t *command)
{
	NrfI2C::write(m_bus, m_addr, command, 3, false);
}


void ads1015LL::read(int& digital_value) {
    //start trigger
    //code gpio trig ;
    //int flag = 1;

    DEBUG_TRACE("BAROMETER_SLEEP_EN is set to HIGH");
     GPIOPins::set(BAROMETER_SLEEP_EN);
    

    DEBUG_TRACE("Flag value before if statement: %d", flag);
    if (flag==1)
    {
      uint8_t write_buffer[3] = {ads1015Command::CONFIG_REG,ads1015Command::MSB_CONFIG,ads1015Command::LSB_CONFIG};
      send_command_conf(write_buffer);
      PMU::delay_ms(10);
      flag = 0;  
    }
    
    uint16_t bin_value = sample_adc(ads1015Command::CONV_REG);
    
    digital_value = (int)(bin_value >> 4);
    DEBUG_TRACE("ads1015LL::read: %f digit", digital_value);
    
}



uint16_t ads1015LL::sample_adc(uint8_t measurement) {
    uint8_t cmd = measurement;  
    send_command(cmd);
    PMU::delay_ms(10);
    uint8_t read_buffer[2];
    NrfI2C::read(m_bus,m_addr,read_buffer,2);
    return ((uint16_t)read_buffer[0] << 8 ) + ((uint16_t)read_buffer[1]);
    
}