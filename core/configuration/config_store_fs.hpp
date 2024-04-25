#pragma once

#include <cstring>
#include "config_store.hpp"
#include "filesystem.hpp"
#include "debug.hpp"
#include "battery.hpp"
#include "dte_params.hpp"
#include "calibration.hpp"



extern BatteryMonitor *battery_monitor;

class LFSConfigurationStore : public ConfigurationStore {

protected:
	bool m_is_pass_predict_valid;
	bool m_is_config_valid;
	BasePassPredict m_pass_predict;

	// Serialization format is as follows:
	//
	// <KEY><BINARY_DATA_ZERO_PADDED_TO_128_BYTES>
	//
	// Note that <KEY> must match the entry at index inside the
	// param_map table otherwise the configuration item is
	// rejected.  <KEY> has fixed length of 5 e.g., "IDT01".

	bool serialize_config_entry(LFSFile &f, unsigned int index) {
		DEBUG_TRACE("TEST2");
		if (index >= MAX_CONFIG_ITEMS)
			return false;
		const BaseMap* entry = &param_map[index];
		struct Serializer {
			uint8_t entry_buffer[BASE_TEXT_MAX_LENGTH];
			void operator()(std::string& s) {
				std::memset(entry_buffer, 0, sizeof(entry_buffer));
				std::memcpy(entry_buffer, s.data(), s.size());
			};
			// BaseGNSSDynModel
			void operator()(unsigned int &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(int &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(double &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(std::time_t &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseArgosMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseArgosPower &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseArgosDepthPile &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(bool &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseGNSSFixMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseGNSSDynModel &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseLEDMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseZoneType &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseArgosModulation &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseUnderwaterDetectSource &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseDebugMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BasePressureSensorLoggingMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseSensorEnableTxMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseRawData &) {
			};
		} s;
		std::visit(s, m_params.at(index));

		return f.write((void *)entry->key.data(), entry->key.size()) == (lfs_ssize_t)entry->key.size() &&
				f.write(s.entry_buffer, sizeof(s.entry_buffer)) == sizeof(s.entry_buffer);
	}

	bool deserialize_config_entry(LFSFile &f, unsigned int index) {

		DEBUG_TRACE("TEST4");	

		if (index >= MAX_CONFIG_ITEMS)
			return false;
		const BaseMap* entry = &param_map[index];
		uint8_t entry_buffer[KEY_LENGTH + BASE_TEXT_MAX_LENGTH];

		// Read entry from file
		if (f.read(entry_buffer, KEY_LENGTH + BASE_TEXT_MAX_LENGTH) != KEY_LENGTH + BASE_TEXT_MAX_LENGTH)
			return false;

		// Check param key matches
		if (std::string((const char *)entry_buffer, KEY_LENGTH) != entry->key)
			return false;

		uint8_t *param_value = &entry_buffer[KEY_LENGTH];

		switch (entry->encoding) {
		case BaseEncoding::DECIMAL:
		{
			int value = *(int *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::AQPERIOD:
		case BaseEncoding::HEXADECIMAL:
		case BaseEncoding::UINT:
		{
			unsigned int value = *(unsigned int *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::TEXT:
		{
			std::string value((const char *)param_value);
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::DATESTRING:
		{
			std::time_t value = *(std::time_t *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::BOOLEAN:
		{
			bool value = *(bool *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::ARGOSFREQ:
		case BaseEncoding::FLOAT:
		{
			double value = *(double *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::DEPTHPILE:
		{
			BaseArgosDepthPile value = *(BaseArgosDepthPile *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::ARGOSMODE:
		{
			BaseArgosMode value = *(BaseArgosMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::ARGOSPOWER:
		{
			BaseArgosPower value = *(BaseArgosPower *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::UWDETECTSOURCE:
		{
			BaseUnderwaterDetectSource value = *(BaseUnderwaterDetectSource *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::GNSSFIXMODE:
		{
			BaseGNSSFixMode value = *(BaseGNSSFixMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::GNSSDYNMODEL:
		{
			BaseGNSSDynModel value = *(BaseGNSSDynModel *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::LEDMODE:
		{
			BaseLEDMode value = *(BaseLEDMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::ZONETYPE:
		{
			BaseZoneType value = *(BaseZoneType *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::MODULATION:
		{
			BaseArgosModulation value = *(BaseArgosModulation *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::DEBUGMODE:
		{
			BaseDebugMode value = *(BaseDebugMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::PRESSURESENSORLOGGINGMODE:
		{
			BasePressureSensorLoggingMode value = *(BasePressureSensorLoggingMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::SENSORENABLETXMODE:
		{
			BaseSensorEnableTxMode value = *(BaseSensorEnableTxMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::KEY_LIST:
		case BaseEncoding::KEY_VALUE_LIST:
		case BaseEncoding::BASE64:
		default:
			return false;
		}

		return true;
	}

	void deserialize_config() {
		DEBUG_TRACE("TEST122133");
		DEBUG_TRACE("ConfigurationStoreLFS::deserialize_config");
		LFSFile f(&m_filesystem, "config.dat", LFS_O_RDONLY);

		unsigned int config_version_code = 0;

		// Read configuration version field
		if (f.read(&config_version_code, sizeof(config_version_code)) != sizeof(config_version_code) ||
				config_version_code != m_config_version_code) {

			DEBUG_WARN("deserialize_config: configuration version mismatch; try to recover protected & reset all others");

			// Try to recover the protected fields
			if (!deserialize_config_entry(f, (unsigned int)ParamID::ARGOS_DECID))
				DEBUG_WARN("deserialize_config: failed to recover ARGOS_DECID");
			else
				DEBUG_TRACE("deserialize_config: recovered ARGOS_DECID");
			if (!deserialize_config_entry(f, (unsigned int)ParamID::ARGOS_HEXID))
				DEBUG_WARN("deserialize_config: failed to recover ARGOS_HEXID");
			else
				DEBUG_TRACE("deserialize_config: recovered ARGOS_HEXID");

			m_requires_serialization = true;
			return;
		}

		for (unsigned int i = 0; i < MAX_CONFIG_ITEMS; i++) {

			if (!deserialize_config_entry(f, i)) {
				DEBUG_WARN("deserialize_config: unable to deserialize param %s - resetting...", param_map[i].name.c_str());
				// Reset parameter to factory default
				m_params.at(i) = default_params.at(i);
				m_requires_serialization = true;
				continue;
			}

			// Check variant index (type) matches default parameters
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_WARN("deserialize_config: param %s variant index mismatch expected %u but got %u - resetting...",
						param_map[i].name.c_str(),
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to factory default
				m_params.at(i) = default_params.at(i);
				m_requires_serialization = true;
				continue;
			}
		}

		m_is_config_valid = true;
	}

	void serialize_config() override {
		DEBUG_TRACE("ConfigurationStoreLFS::serialize_config");
		DEBUG_TRACE("TEST A");
		
		LFSFile f(&m_filesystem, "config.dat", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
		DEBUG_TRACE("TEST B");
		// Write configuration version field
		if (f.write((void *)&m_config_version_code, sizeof(m_config_version_code)) != sizeof(m_config_version_code)) {
			DEBUG_TRACE("TEST C");
			throw CONFIG_STORE_CORRUPTED;
		}	
		for (unsigned int i = 0; i < MAX_CONFIG_ITEMS; i++) {

			// Check variant index (type) matches default parameter
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_WARN("serialize_config: param %s variant index mismatch expected %u but got %u - resetting...",
						param_map[i].name.c_str(),
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to back to factory default
				m_params.at(i) = default_params.at(i);
			}

			if (!serialize_config_entry(f, i)) {
				DEBUG_ERROR("serialize_config: failed to serialize param %u", i);
				throw CONFIG_STORE_CORRUPTED;
			}
		}

		m_is_config_valid = true;

		DEBUG_TRACE("ConfigurationStoreLFS::serialize_config: saved new file config.data");
	}

	void deserialize_prepass() {
		DEBUG_TRACE("ConfigurationStoreLFS::deserialize_prepass");
		LFSFile f(&m_filesystem, "pass_predict.dat", LFS_O_RDWR);
		if (f.read(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict)) {
			if (m_pass_predict.version_code == m_config_version_code_aop)
				m_is_pass_predict_valid = true;
			else
				DEBUG_WARN("ConfigurationStoreLFS::deserialize_prepass: pass predict file version code mismatch");
		}
		if (!m_is_pass_predict_valid) {
			DEBUG_TRACE("ConfigurationStoreLFS::deserialize_prepass: using default AOP");
			create_default_prepass();
		}
	}

	void serialize_pass_predict() {
		DEBUG_TRACE("ConfigurationStoreLFS::serialize_pass_predict");
		LFSFile f(&m_filesystem, "pass_predict.dat", LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC);
		m_pass_predict.version_code = m_config_version_code_aop;
		m_is_pass_predict_valid = false;
		m_is_pass_predict_valid = f.write(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict);
		if (!m_is_pass_predict_valid) {
			DEBUG_ERROR("serialize_pass_predict: failed to serialize pass predict");
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	void create_default_prepass() {
		DEBUG_TRACE("ConfigurationStoreLFS::create_default_prepass");
		write_pass_predict((BasePassPredict&)default_prepass);
	}

	void update_battery_level() override {
		battery_monitor->update();
		m_battery_level = battery_monitor->get_level();
		m_battery_voltage = battery_monitor->get_voltage();
		m_is_battery_level_low = battery_monitor->is_battery_low();
	}

private:
	FileSystem &m_filesystem;
	bool        m_requires_serialization;

	void serialize_protected_config() {
		LFSFile f(&m_filesystem, "config.dat", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

		// Write configuration version field
		if (f.write((void *)&m_config_version_code, sizeof(m_config_version_code)) != sizeof(m_config_version_code))
			throw CONFIG_STORE_CORRUPTED;

		for (unsigned int i = 0; i <= (unsigned int)ParamID::ARGOS_HEXID ; i++) {
			// Check variant index (type) matches default parameter
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_TRACE("serialize_config: protected param %u variant index mismatch expected %u but got %u - repairing",
						i,
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to back to factory default
				m_params.at(i) = default_params.at(i);
			}

			if (!serialize_config_entry(f, i)) {
				DEBUG_TRACE("serialize_config: failed to serialize protected param %u", i);
				throw CONFIG_STORE_CORRUPTED;
			}
		}

		DEBUG_TRACE("ConfigurationStoreLFS::serialize_protected_config: saved protected params to config.data");
	}

	void save_calibration_data() {
		CalibratableManager::save_all(true);
	}

public:
	LFSConfigurationStore(FileSystem &filesystem) : m_is_pass_predict_valid(false), m_is_config_valid(false), m_filesystem(filesystem) {}

	void init() override {
		m_requires_serialization = false;
		m_is_pass_predict_valid = false;
		m_is_config_valid = false;

		// Copy default params so we have an initial working set
		m_params = default_params;

		// Read in configuration file or create new one if it doesn't not exist
		try {
			deserialize_config();
		} catch (int e) {
			DEBUG_WARN("No configuration file so creating a default file");
			m_requires_serialization = true;
		}

		if (m_requires_serialization)
			serialize_config();

		if (!m_is_config_valid)
			throw CONFIG_STORE_CORRUPTED; // This is a non-recoverable error

		// Read in prepass file
		try {
			deserialize_prepass();
		} catch (int e) {
			DEBUG_WARN("AOP file does not exist or is corrupted - resetting AOP file");
			create_default_prepass();
		}
	}

	bool is_valid() override {
		return m_is_config_valid;
	}

	void factory_reset() override {
		m_filesystem.umount();
		m_filesystem.format();
		m_filesystem.mount();
		serialize_protected_config(); // Recover "protected" parameters
		save_calibration_data();
		m_is_config_valid = false;
		m_is_pass_predict_valid = false;
	}

	BasePassPredict& read_pass_predict() override {
		if (m_is_pass_predict_valid) {
			return m_pass_predict;
		}
		throw CONFIG_DOES_NOT_EXIST;
	}

	void write_pass_predict(BasePassPredict& value) override {
		m_pass_predict = value;
		serialize_pass_predict();
	}
};
