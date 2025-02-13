#include "bsp.hpp"
#include "pmu.hpp"
#include "ota_file_updater.hpp"
#include "logger.hpp"
#include "config_store.hpp"
#include "service_scheduler.hpp"
#include "scheduler.hpp"
#include "service.hpp"
#include "dte_handler.hpp"
#include "filesystem.hpp"
#include "config_store.hpp"
#include "error.hpp"
#include "timer.hpp"
#include "debug.hpp"
#include "switch.hpp"
#include "reed.hpp"
#include "ledsm.hpp"
#include "battery.hpp"
#include "gps.hpp"
#include "ble_service.hpp"
#include "gentracker.hpp"

// These contexts must be created before the FSM is initialised
extern FileSystem *main_filesystem;
extern Scheduler *system_scheduler;
extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern BLEService *ble_service;
extern OTAFileUpdater *ota_updater;
extern DTEHandler *dte_handler;
extern ReedSwitch *reed_switch;
extern BatteryMonitor *battery_monitor;
extern BaseDebugMode g_debug_mode;

// FSM initial state -> LEDOff
FSM_INITIAL_STATE(LEDState, LEDOff);

using led_handle = LEDState;


void GenTracker::react(tinyfsm::Event const &) { }

void GenTracker::react(ReedSwitchEvent const &event)
{
	DEBUG_INFO("react: ReedSwitchEvent: %u", (int)event.state);

	// Reed switch event handling:
	// ENGAGE -- engaged LED state
	// RELEASE -- disengage LED state
	// SHORT_HOLD - configuration state
	// LONG_HOLD -- power off
	if (event.state == ReedSwitchGesture::ENGAGE) {
		led_handle::dispatch<SetLEDMagnetEngaged>({});
	} else if (event.state == ReedSwitchGesture::RELEASE) {
		led_handle::dispatch<SetLEDMagnetDisengaged>({});
		if (!is_in_state<OffState>()) {
			if (led_handle::is_in_state<LEDPreOperationalPending>())
				transit<PreOperationalState>();
			else if (led_handle::is_in_state<LEDConfigPending>())
				transit<ConfigurationState>();
		}
	} else if (event.state == ReedSwitchGesture::SHORT_HOLD) {
		if (is_in_state<ConfigurationState>())
			led_handle::dispatch<SetLEDPreOperationalPending>({});
		else
			led_handle::dispatch<SetLEDConfigPending>({});
	} else if (event.state == ReedSwitchGesture::LONG_HOLD) {
		led_handle::dispatch<SetLEDMagnetDisengaged>({});
		transit<OffState>();
	}
}

void GenTracker::react(ErrorEvent const &event) {
	DEBUG_ERROR("GenTracker::react: ErrorEvent: error_code=%u", event.error_code);
	transit<ErrorState>();
}

void GenTracker::entry(void) { };
void GenTracker::exit(void)  { };

void GenTracker::notify_bad_filesystem_error() {
	ErrorEvent event;
	event.error_code = BAD_FILESYSTEM;
	dispatch(event);
}

void GenTracker::kick_watchdog() {
	DEBUG_TRACE("GenTracker::kick_watchdog: calling PMU::kick_watchdog");
	PMU::kick_watchdog();
	system_scheduler->post_task_prio([](){
		kick_watchdog();
	}, "KickWatchdog", Scheduler::DEFAULT_PRIORITY, BSP::WDT_Inits[BSP::WDT].config.reload_value * 0.90);
}

void BootState::entry() {

	DEBUG_INFO("entry: BootState");

	// Ensure the system timer is started to allow scheduling to work
	system_timer->start();

	// Turn status LED white to indicate boot up
	led_handle::start();
	led_handle::dispatch<SetLEDBoot>({});

	// If we can't mount the filesystem then try to format it first and retry
	if (!main_filesystem->is_mounted() && main_filesystem->mount() < 0)
	{
		DEBUG_TRACE("format filesystem");
		if (main_filesystem->format() < 0 || main_filesystem->mount() < 0)
		{
			// We can't mount a formatted filesystem, something bad has happened
			system_scheduler->post_task_prio(notify_bad_filesystem_error, "GenTrackerFileSystemError");
			return;
		}
	}

	// Start reed switch monitoring and dispatch events to state machine
	reed_switch->start([](ReedSwitchGesture s) { ReedSwitchEvent e; e.state = s; dispatch(e); });

	try {
		// The underlying classes will create the files on the filesystem if they do not
		// already yet exist
		LoggerManager::create();
		configuration_store->init();
	    DEBUG_INFO("Firmware Version: %s", FW_APP_VERSION_STR_C);
		LoggerManager::show_info();
		DEBUG_INFO("configuration_store: is_valid=%u", configuration_store->is_valid());
		DEBUG_INFO("reset cause: %s", PMU::reset_cause().c_str());
		PMU::print_stack();
		// Transition to PreOperational state after initialisation
		system_scheduler->post_task_prio([this](){
			kick_watchdog();
			transit<PreOperationalState>();
		},
		"GenTrackerBootStateTransitPreOperationalState",
		Scheduler::DEFAULT_PRIORITY,
		1000);
	} catch (...) {
		system_scheduler->post_task_prio(notify_bad_filesystem_error, "GenTrackerFilesystemError");
	}
}

void BootState::exit() {

	DEBUG_INFO("exit: BootState");

	// Turn status LED off to indicate exit from boot state
	led_handle::dispatch<SetLEDOff>({});
}

void OffState::entry() {
	DEBUG_INFO("entry: OffState");
	led_handle::dispatch<SetLEDPowerDown>({});
	m_off_state_task = system_scheduler->post_task_prio([](){
		led_handle::dispatch<SetLEDOff>({});
		PMU::powerdown();
	},
	"GenTrackerOffStateTransitPowerDown",
	Scheduler::DEFAULT_PRIORITY, OFF_LED_PERIOD_MS);
}

void OffState::exit() {
	DEBUG_INFO("exit: OffState");
	system_scheduler->cancel_task(m_off_state_task);
	led_handle::dispatch<SetLEDOff>({});
}

void PreOperationalState::entry() {
	DEBUG_INFO("entry: PreOperationalState");
	if (configuration_store->is_valid()) {
		// Force battery monitor to update its levels
		battery_monitor->update();
		if (battery_monitor->is_battery_critical()) {
			transit<BatteryCriticalState>();
			return;
		}

		if (battery_monitor->is_battery_low())
			led_handle::dispatch<SetLEDPreOperationalBatteryLow>({});
		else
			led_handle::dispatch<SetLEDPreOperationalBatteryNominal>({});

		m_preop_state_task = system_scheduler->post_task_prio([this](){
			transit<OperationalState>();
		},
		"GenTrackerPreOperationalStateTransitOperationalState",
		Scheduler::DEFAULT_PRIORITY, TRANSIT_PERIOD_MS);
	} else {
		led_handle::dispatch<SetLEDPreOperationalError>({});
		m_preop_state_task = system_scheduler->post_task_prio([this](){
			transit<ErrorState>();
		},
		"GenTrackerPreOperationalStateTransitErrorState",
		Scheduler::DEFAULT_PRIORITY, TRANSIT_PERIOD_MS);
	}
}

void PreOperationalState::exit() {
	DEBUG_INFO("exit: PreOperationalState");
	system_scheduler->cancel_task(m_preop_state_task);
	led_handle::dispatch<SetLEDOff>({});
}

void OperationalState::entry() {
	DEBUG_INFO("entry: OperationalState");

	battery_monitor->subscribe(*this);
	led_handle::dispatch<SetLEDOff>({});

	ServiceManager::startall([this](ServiceEvent& e) {
		service_event_handler(e);
	});

	BaseDebugMode debug_mode = configuration_store->read_param<BaseDebugMode>(ParamID::DEBUG_OUTPUT_MODE);
	if (debug_mode == BaseDebugMode::BLE_NUS) {
		set_ble_device_name();
		ble_service->start([](BLEServiceEvent&){ return 0; });
		g_debug_mode = debug_mode;
	}
}

void OperationalState::react(BatteryMonitorEventVoltageCritical const &) {
	DEBUG_INFO("OperationalState::react: BatteryMonitorEventVoltageCritical");
	system_scheduler->post_task_prio([this]() {
		transit<BatteryCriticalState>();
	}, "BatteryCriticalHandler", Scheduler::DEFAULT_PRIORITY, 1000);
}

void OperationalState::service_event_handler(ServiceEvent& e) {

	// Notify event to all peer services
	ServiceManager::notify_peer_event(e);

	if (e.event_source == ServiceIdentifier::GNSS_SENSOR) {
		if (e.event_type == ServiceEventType::SERVICE_ACTIVE) {
			led_handle::dispatch<SetLEDGNSSOn>({});
		} else if (e.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			if (std::get<GPSLogEntry>(e.event_data).info.valid)
				led_handle::dispatch<SetLEDGNSSOffWithFix>({});
			else
				led_handle::dispatch<SetLEDGNSSOffWithoutFix>({});
		}
		return;
	}
	else if (e.event_source == ServiceIdentifier::ARGOS_TX) {
		// New Argos TX event handling
		if (e.event_type == ServiceEventType::SERVICE_ACTIVE) {
			led_handle::dispatch<SetLEDArgosTX>({});
		}
		else if (e.event_type == ServiceEventType::SERVICE_INACTIVE) {
			led_handle::dispatch<SetLEDArgosTXComplete>({});
		}
	}
}

void OperationalState::exit() {
	DEBUG_INFO("exit: OperationalState");
	led_handle::dispatch<SetLEDOff>({});
	ServiceManager::stopall();
	BaseDebugMode debug_mode = configuration_store->read_param<BaseDebugMode>(ParamID::DEBUG_OUTPUT_MODE);
	if (debug_mode == BaseDebugMode::BLE_NUS) {
		g_debug_mode = BaseDebugMode::UART;
		ble_service->stop();
		DEBUG_TRACE("exit: OperationalState: BLE service stopped");
		PMU::delay_ms(100);
	}
	battery_monitor->unsubscribe(*this);
}

void ConfigurationState::entry() {
	DEBUG_INFO("entry: ConfigurationState");

	// Flash the blue LED to indicate we have started BLE and we are
	// waiting for a connection
	led_handle::dispatch<SetLEDConfigNotConnected>({});

	set_ble_device_name();
	ble_service->start([this](BLEServiceEvent& event) -> int { return on_ble_event(event); } );
	restart_inactivity_timeout();
}

void ConfigurationState::exit() {
	DEBUG_INFO("exit: ConfigurationState");
	system_scheduler->cancel_task(m_ble_inactivity_timeout_task);
	ble_service->stop();
	led_handle::dispatch<SetLEDOff>({});
}

void GenTracker::set_ble_device_name() {
	std::string device_model = configuration_store->read_param<std::string>(ParamID::DEVICE_MODEL);
#if ARGOS_EXT
	unsigned int identifier = configuration_store->read_param<unsigned int>(ParamID::DEVICE_DECID);
#else
	unsigned int identifier = configuration_store->read_param<unsigned int>(ParamID::ARGOS_DECID);
#endif
	std::string device_name = device_model + " " + std::to_string(identifier);
	DEBUG_TRACE("GenTracker::set_ble_device_name: %s", device_name.c_str());
	ble_service->set_device_name(device_name);
}

int ConfigurationState::on_ble_event(BLEServiceEvent& event) {
	int rc = 0;

	switch (event.event_type) {
	case BLEServiceEventType::CONNECTED:
		DEBUG_TRACE("ConfigurationState::on_ble_event: CONNECTED");
		// Indicate DTE connection is made
		dte_handler->reset_state();
		led_handle::dispatch<SetLEDConfigConnected>({});
		restart_inactivity_timeout();
		break;
	case BLEServiceEventType::DISCONNECTED:
		DEBUG_TRACE("ConfigurationState::on_ble_event: DISCONNECTED");
		ota_updater->abort_file_transfer();
		led_handle::dispatch<SetLEDConfigNotConnected>({});
		break;
	case BLEServiceEventType::DTE_DATA_RECEIVED:
		DEBUG_TRACE("ConfigurationState::on_ble_event: DTE_DATA_RECEIVED");
		restart_inactivity_timeout();
		system_scheduler->post_task_prio(std::bind(&ConfigurationState::process_received_data, this),
				"BLEProcessReceivedData");
		break;
	case BLEServiceEventType::OTA_START:
		DEBUG_INFO("ConfigurationState::on_ble_event: OTA_START");
		restart_inactivity_timeout();
		ota_updater->start_file_transfer((OTAFileIdentifier)event.file_id, event.file_size, event.crc32);
		break;
	case BLEServiceEventType::OTA_END:
		DEBUG_INFO("ConfigurationState::on_ble_event: OTA_END");
		restart_inactivity_timeout();
		ota_updater->complete_file_transfer();
		system_scheduler->post_task_prio(std::bind(&OTAFileUpdater::apply_file_update, ota_updater),
				"BLEApplyOTAFileUpdate"
				);
		break;
	case BLEServiceEventType::OTA_ABORT:
		DEBUG_INFO("ConfigurationState::on_ble_event: OTA_ABORT");
		restart_inactivity_timeout();
		ota_updater->abort_file_transfer();
		break;
	case BLEServiceEventType::OTA_FILE_DATA:
		//DEBUG_TRACE("ConfigurationState::on_ble_event: OTA_FILE_DATA");
		restart_inactivity_timeout();
		ota_updater->write_file_data(event.data, event.length);
		break;
	default:
		break;
	}

	return rc;
}


void ConfigurationState::on_ble_inactivity_timeout() {
	DEBUG_INFO("BLE Inactivity Timeout");
	transit<OffState>();
}

void ConfigurationState::restart_inactivity_timeout() {
	//DEBUG_TRACE("Restart BLE inactivity timeout: %lu", system_timer->get_counter());
	system_scheduler->cancel_task(m_ble_inactivity_timeout_task);
	m_ble_inactivity_timeout_task = system_scheduler->post_task_prio(std::bind(&ConfigurationState::on_ble_inactivity_timeout, this),
			"BLEInactivityTimeout",
			Scheduler::DEFAULT_PRIORITY, BLE_INACTIVITY_TIMEOUT_MS);
}

void ConfigurationState::process_received_data() {
	auto req = ble_service->read_line();

	if (req.size())
	{
		DEBUG_TRACE("received %u bytes:", req.size());
#if defined(DEBUG_ENABLE) && DEBUG_LEVEL >= 4
		printf("%s\n", req.c_str());
#endif

		std::string resp;
		DTEAction action;

		do
		{
			action = dte_handler->handle_dte_message(req, resp);
			if (resp.size())
			{
				DEBUG_TRACE("responding: %s", resp.c_str());
				if (!ble_service->write(resp)) {
					dte_handler->reset_state();
					break;
				}

				// Reset inactivity timeout whenever we send a response
				// This is important during a command sequences that can take
				// a long time to complete (eg DUMPD)
				restart_inactivity_timeout();

				// We must also kick the watchdog here since the main scheduler
				// is being deferred
				PMU::kick_watchdog();
			}

			if (action == DTEAction::FACTR)
			{
				DEBUG_INFO("Perform factory reset of configuration store");
				configuration_store->factory_reset();

				// After formatting the filesystem we must do a system reset so that
				// the boot up procedure can repopulate files
				PMU::reset(false);
			}
			else if (action == DTEAction::RESET)
			{
				DEBUG_INFO("Perform device reset");

				// Execute this after 3 seconds to allow time for the BLE response to be sent
				system_scheduler->post_task_prio([](){
					PMU::reset(false);
				},
				"DTEActionPMUReset",
				Scheduler::DEFAULT_PRIORITY, 3000);
			}
			else if (action == DTEAction::SECUR)
			{
				// TODO: add secure procedure
				DEBUG_INFO("Perform secure procedure");
			}
			else if (action == DTEAction::CONFIG_UPDATED)
			{
				// TODO: reserved for future use
			}

		} while (action == DTEAction::AGAIN);
	}
}

void BatteryCriticalState::entry() {
	DEBUG_INFO("entry: BatteryCriticalState");
	led_handle::dispatch<SetLEDBatteryCritical>({});
	m_transit_task = system_scheduler->post_task_prio([this](){
		transit<OffState>();
	},
	"GenTrackerBatteryCriticalTransitOffState",
	Scheduler::DEFAULT_PRIORITY, BATTERY_CRITICAL_TIMEOUT_MS);
}

void BatteryCriticalState::exit() {
	DEBUG_INFO("exit: BatteryCriticalState");
	system_scheduler->cancel_task(m_transit_task);
	led_handle::dispatch<SetLEDOff>({});
}

void ErrorState::entry() {
	DEBUG_INFO("entry: ErrorState");
	led_handle::dispatch<SetLEDError>({});
	m_shutdown_task = system_scheduler->post_task_prio([this](){
		transit<OffState>();
	},
	"GenTrackerErrorStateTransitOffState",
	Scheduler::DEFAULT_PRIORITY, 5000);
}

void ErrorState::exit() {
	DEBUG_INFO("exit: ErrorState");
	system_scheduler->cancel_task(m_shutdown_task);
	led_handle::dispatch<SetLEDOff>({});
}
