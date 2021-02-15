#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_m8q.hpp"
#include "fake_rtc.hpp"
#include "fake_config_store.hpp"
#include "fake_logger.hpp"
#include "fake_timer.hpp"
#include "dte_protocol.hpp"

extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern LocationScheduler *location_scheduler;
extern Scheduler *system_scheduler;
extern Logger *sensor_log;
extern RTC *rtc;

TEST_GROUP(GpsScheduler)
{
	FakeConfigurationStore *fake_config_store;
	GPSScheduler *gps_sched;
	MockM8Q *mock_m8q;
	FakeRTC *fake_rtc;
	FakeTimer *fake_timer;
	FakeLog *fake_log;

	void setup() {
		mock_m8q = new MockM8Q;
		gps_sched = mock_m8q;
		location_scheduler = gps_sched;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		fake_log = new FakeLog;
		sensor_log = fake_log;
		fake_rtc = new FakeRTC;
		rtc = fake_rtc;
		fake_timer = new FakeTimer;
		system_timer = fake_timer;
		system_scheduler = new Scheduler(system_timer);
		m_current_ms = 0;
	}

	void teardown() {
		delete system_scheduler;
		delete fake_timer;
		delete fake_rtc;
		delete fake_log;
		delete fake_config_store;
		delete mock_m8q;
	}

	void increment_time_ms(uint64_t ms)
	{
		while (ms)
		{
			m_current_ms++;

			if (m_current_ms % 1000 == 0)
				fake_rtc->incrementtime(1);
			fake_timer->increment_counter(1);

			system_scheduler->run();

			ms--;
		}
	}

	void increment_time_s(uint64_t s)
	{
		increment_time_ms(s * 1000);
	}

	void increment_time_min(uint64_t min)
	{
		increment_time_ms(min * 60 * 1000);
	}

	uint64_t m_current_ms;
};


TEST(GpsScheduler, GNSSDisabled)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = false;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_10_MINS;
	unsigned int gnss_acq_timeout = 0;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	location_scheduler->start();

	increment_time_min(60);
}

TEST(GpsScheduler, GNSSEnabled10MinutesNoFixB)
{
	int iterations = 3;
	unsigned int period_minutes = 10;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_10_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled15MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 15;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_15_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled30MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 30;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_30_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled60MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 60;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_60_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled120MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 120;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_120_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled360MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 360;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_360_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled720MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 720;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_720_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled1440MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 1440;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_1440_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s((period_minutes * 60) - 1);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}

TEST(GpsScheduler, GNSSEnabled10MinutesNoFixOffSchedule)
{
	int iterations = 3;
	unsigned int period_minutes = 10;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	BaseAqPeriod dloc_arg_nom = BaseAqPeriod::AQPERIOD_10_MINS;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(1580083500); // 27/01/2020 00:05:00

	location_scheduler->start();

	// We're expecting the device to turn on at 27/01/2020 00:10:00
	increment_time_min(4);

	mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
	increment_time_min(1);

	mock().expectOneCall("power_off").onObject(gps_sched);
	increment_time_s(gnss_acq_timeout);

	for (int i = 0; i < iterations; ++i)
	{
		increment_time_s((period_minutes * 60) - gnss_acq_timeout - 1);

		mock().expectOneCall("power_on").onObject(gps_sched).ignoreOtherParameters();
		increment_time_s(1);

		mock().expectOneCall("power_off").onObject(gps_sched);
		increment_time_s(gnss_acq_timeout);
	}
}