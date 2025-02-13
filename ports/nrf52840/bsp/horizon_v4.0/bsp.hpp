#pragma once

#include <cstdint>
#include "sdk_config.h"
#include "nrfx_uarte.h"
#include "nrfx_qspi.h"
#include "drv_rtc.h"
#include "nrfx_spim.h"
#include "nrfx_gpiote.h"
#include "nrfx_saadc.h"
#include "nrf_gpio.h"
#include "nrfx_twim.h"
#include "nrfx_wdt.h"
#include "nrf_libuarte_async.h"

// Logical device mappings to physical devices
#define RTC_DATE_TIME  BSP::RTC::RTC_1
#define RTC_TIMER      BSP::RTC::RTC_2
#define SPI_SATELLITE  BSP::SPI::SPI_2
#define BATTERY_ADC	   BSP::ADC::ADC_CHANNEL_0
#define SWS_ENABLE_PIN BSP::GPIO::GPIO_SWS_EN
#define SWS_SAMPLE_PIN BSP::GPIO::GPIO_SWS
#define UART_GPS	   BSP::UART::UART_0
#define REED_SWITCH_ACTIVE_STATE   false
#define EXT_LED_PIN    BSP::GPIO::GPIO_EXT2_GPIO6
#define EXT_I2C_BUS     BSP::I2C::I2C_0
#define SAT_PWR_EN     BSP::GPIO::GPIO_EXT2_GPIO1
#define SAT_RESET      BSP::GPIO::GPIO_EXT2_GPIO2
#define ONBOARD_I2C_BUS BSP::I2C::I2C_1
#define BMX160_WAKEUP_PIN  BSP::GPIO::GPIO_INT1_AG
#define GPS_POWER		BSP::GPIO::GPIO_GPS_EXT_INT
#define AG_ENABLE		BSP::GPIO::GPIO_DEN_AG

// I2C device mappings
#define MS5803_VARIANT "MS5803_14BA"
#define MS5837_VARIANT "MS5837_30BA"
#define LIGHT_DEVICE   EXT_I2C_BUS
#define OEM_PH_DEVICE  EXT_I2C_BUS
#define OEM_RTD_DEVICE EXT_I2C_BUS
#define EZO_RTD_DEVICE ONBOARD_I2C_BUS
#define CDT_MS5803_DEVICE EXT_I2C_BUS
#define CDT_AD5933_DEVICE EXT_I2C_BUS
#define BMX160_DEVICE  ONBOARD_I2C_BUS

// Device addresses
#define MS5803_ADDRESS	0x77
#define MS5837_ADDRESS	0x76
#define AD5933_ADDRESS  0x0D
#define LIGHT_DEVICE_ADDR   0x29
#define OEM_PH_DEVICE_ADDR  0x65
#define EZO_RTD_DEVICE_ADDR 0x66
#define OEM_RTD_DEVICE_ADDR 0x68
#define BMX160_ADDRESS  0x69

// Battery voltage ADC gain
#define ADC_GAIN        (1.0f/6.0f)  // 1/6 gain
#define RP506_ADC_GAIN  1.67f

// Power on requires reed switch for 3 seconds
#define POWER_ON_RESET_REQUIRES_REED_SWITCH
#define PSEUDO_POWER_OFF

// Other options
#define HW_VERSION_DETECT 0
#define NO_GPS_POWER_REG  1
#define NO_ARGOS_PA_GAIN_CTRL 1
#define ARGOS_EXT 1

// If Artic board is detected then don't allow EXT I2C bus to be used
#define ARTIC_I2C_BUS_CONFLICT EXT_I2C_BUS
#define ARTIC_EXT_LED_CONFLICT EXT_LED_PIN

namespace BSP
{
	///////////////////////////////// GPIO definitions ////////////////////////////////
	enum GPIO
	{
		GPIO_DEBUG,
		GPIO_EXT2_GPIO1,
		GPIO_EXT2_GPIO2,
		GPIO_EXT2_GPIO3,
		GPIO_EXT2_GPIO4,
		GPIO_EXT2_GPIO5,
		GPIO_EXT2_GPIO6,
		GPIO_EXT1_GPIO1,
		GPIO_EXT1_GPIO2,
		GPIO_EXT1_GPIO3,
		GPIO_SWS,
		GPIO_SWS_EN,
		GPIO_REED_SW,
		GPIO_GPOUT,
		GPIO_LED_GREEN,
		GPIO_LED_RED,
		GPIO_LED_BLUE,
		GPIO_INT_M,
		GPIO_DEN_AG,
		GPIO_INT1_AG,
		GPIO_INT2_AG,
		GPIO_FLASH_IO2,
		GPIO_FLASH_IO3,
		GPIO_GPS_EXT_INT,
		GPIO_DFU_BOOT,
		GPIO_TOTAL_NUMBER
	};

	typedef struct
	{
		uint32_t             pin_number;
		nrf_gpio_pin_dir_t   dir;
		nrf_gpio_pin_input_t input;
		nrf_gpio_pin_pull_t  pull;
		nrf_gpio_pin_drive_t drive;
		nrf_gpio_pin_sense_t sense;
		nrfx_gpiote_in_config_t gpiote_in_config;
	} GPIO_InitTypeDefAndInst_t;

	extern const GPIO_InitTypeDefAndInst_t GPIO_Inits[GPIO_TOTAL_NUMBER];

	// Interrupt priorities (0, 1, 4  are reserved for the softdevice)
    static constexpr uint8_t INTERRUPT_PRIORITY_WDT       = 2;
    static constexpr uint8_t INTERRUPT_PRIORITY_RTC_1     = 2;
    static constexpr uint8_t INTERRUPT_PRIORITY_RTC_2     = 2;
    static constexpr uint8_t INTERRUPT_PRIORITY_UART_0    = 3;
    static constexpr uint8_t INTERRUPT_PRIORITY_UART_1    = 3;
    static constexpr uint8_t INTERRUPT_PRIORITY_I2C_0     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_I2C_1     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_0     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_1     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_2     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_3     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_QSPI_0    = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_0   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_1   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_2   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_3   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_4   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_0     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_1     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_2     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_3     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_ADC       = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_RTC_0     = 6;

    ////////////////////////////////// UART definitions /////////////////////////////////

    enum UART
    {
    #if NRFX_UARTE0_ENABLED
        UART_0,
    #endif
    #if NRFX_UARTE1_ENABLED
        UART_1,
    #endif
        UART_TOTAL_NUMBER
    };

    struct UART_InitTypeDefAndInst
    {
        nrfx_uarte_t uarte;
        nrfx_uarte_config_t config;
    };

    extern const UART_InitTypeDefAndInst UART_Inits[UART_TOTAL_NUMBER];

    ////////////////////////////////// QSPI definitions /////////////////////////////////

    enum QSPI
    {
    #ifdef NRFX_QSPI_ENABLED
        QSPI_0,
    #endif
        QSPI_TOTAL_NUMBER
    };

    struct QSPI_InitTypeDefAndInst
    {
        nrfx_qspi_config_t config;
    };

    extern const QSPI_InitTypeDefAndInst QSPI_Inits[QSPI_TOTAL_NUMBER];

    ////////////////////////////////// RTC definitions /////////////////////////////////
    enum RTC
    {
#if APP_TIMER_V2_RTC0_ENABLED
    	RTC_RESERVED, // Reserved by the softdevice
#endif
#if APP_TIMER_V2_RTC1_ENABLED
		RTC_1,
#endif
#if APP_TIMER_V2_RTC2_ENABLED
		RTC_2,
#endif
		RTC_TOTAL_NUMBER
    };

    typedef struct
    {
    	drv_rtc_t rtc;
        uint8_t irq_priority;
    } RTC_InitTypeDefAndInst_t;

    extern const RTC_InitTypeDefAndInst_t RTC_Inits[RTC_TOTAL_NUMBER];

    ////////////////////////////////// SPI definitions /////////////////////////////////

    enum SPI
    {
#if NRFX_SPIM0_ENABLED
    	SPI_0,
#endif
#if NRFX_SPIM1_ENABLED
		SPI_1,
#endif
#if NRFX_SPIM2_ENABLED
		SPI_2,
#endif
#if NRFX_SPIM3_ENABLED
		SPI_3,
#endif
		SPI_TOTAL_NUMBER
    };

    typedef struct
    {
        nrfx_spim_t spim;
        nrfx_spim_config_t config;
    } SPI_InitTypeDefAndInst_t;

    extern const SPI_InitTypeDefAndInst_t SPI_Inits[SPI_TOTAL_NUMBER];

    ////////////////////////////////// ADC definitions /////////////////////////////////

    enum ADC
    {
        ADC_CHANNEL_0,
        ADC_TOTAL_CHANNELS
    };

    typedef struct
    {
    	nrfx_saadc_config_t config;
        nrf_saadc_channel_config_t channel_config[ADC_TOTAL_CHANNELS];
    } ADC_InitTypeDefAndInst_t;

    extern const ADC_InitTypeDefAndInst_t ADC_Inits;

    ////////////////////////////////// I2C definitions /////////////////////////////////
    enum I2C
	{
#if NRFX_TWIM0_ENABLED
    	I2C_0,
#endif
#if NRFX_TWIM1_ENABLED
		I2C_1,
#endif
		I2C_TOTAL_NUMBER
	};

    typedef struct
    {
        nrfx_twim_t twim;
        nrfx_twim_config_t twim_config;
    } I2C_InitTypeDefAndInst_t;

    extern const I2C_InitTypeDefAndInst_t I2C_Inits[I2C_TOTAL_NUMBER];

    ////////////////////////////////// WDT definitions /////////////////////////////////
    enum WDT
	{
#if NRFX_WDT_ENABLED
    	WDT,
#endif
		WDT_TOTAL_NUMBER
	};

    typedef struct
    {
    	nrfx_wdt_config_t config;
    } WDT_InitTypeDefAndInst_t;

    extern const WDT_InitTypeDefAndInst_t WDT_Inits[WDT_TOTAL_NUMBER];

    ///////////////////////////// ASYNC UART definitions //////////////////////////////

     typedef struct
     {
         const nrf_libuarte_async_t *uart;
         const nrf_libuarte_async_config_t config;
     } UARTAsync_InitTypeDefAndInst_t;

     extern const UARTAsync_InitTypeDefAndInst_t UARTAsync_Inits[1];
}
