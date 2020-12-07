#include "dte_protocol.hpp"
#include "config_store.hpp"
#include "memory_access.hpp"

using namespace std::literals::string_literals;


enum class DTEAction {
	NONE,    // Default action is none
	RESET,   // Deferred action since DTE must respond first before reset can be performed
	FACTR,   // Deferred action since DTE must respond first before factory reset can be performed
	SECUR    // DTE service must be notified when SECUR is requested to grant privileges for OTA FW commands
};


extern ConfigurationStore *configuration_store;
extern MemoryAccess *memory_access;


class DTEHandler {
private:
	enum class DTEError {
		OK,
		INCORRECT_COMMAND,
		NO_LENGTH_DELIMITER,
		NO_DATA_DELIMITER,
		DATA_LENGTH_MISMATCH,
		INCORRECT_DATA
	};

	static std::string PARML_REQ(int error_code) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARML_RESP, error_code);
		}

		// Build up a list of all implemented parameters
		std::vector<ParamID> params;
		for (unsigned int i = 0; i < sizeof(param_map)/sizeof(BaseMap); i++) {
			if (param_map[i].is_implemented) {
				params.push_back(static_cast<ParamID>(i));
			}
		}

		return DTEEncoder::encode(DTECommand::PARML_RESP, params);
	}

	static std::string PARMW_REQ(int error_code, std::vector<ParamValue>& param_values) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARML_RESP, error_code);
		}

		for(unsigned int i = 0; i < param_values.size(); i++)
			configuration_store->write_param(param_values[i].param, param_values[i].value);

		return DTEEncoder::encode(DTECommand::PARMW_RESP, DTEError::OK);
	}

	static std::string PARMR_REQ(int error_code, std::vector<ParamID>& params) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARMR_RESP, error_code);
		}

		std::vector<ParamValue> param_values;
		for (unsigned int i = 0; i < params.size(); i++) {
			BaseType x = configuration_store->read_param<BaseType>(params[i]);
			ParamValue p = {
				params[i],
				x
			};
			param_values.push_back(p);
		}

		return DTEEncoder::encode(DTECommand::PARMR_RESP, param_values);
	}

	static std::string PROFW_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (!error_code) {
			configuration_store->write_param(ParamID::PROFILE_NAME, arg_list[0]);
		}

		return DTEEncoder::encode(DTECommand::PROFW_RESP, error_code);
	}

	static std::string PROFR_REQ(int error_code) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PROFR_RESP, error_code);
		}

		return DTEEncoder::encode(DTECommand::PROFR_RESP, error_code, configuration_store->read_param<std::string>(ParamID::PROFILE_NAME));
	}

	static std::string SECUR_REQ(int error_code) {

		return DTEEncoder::encode(DTECommand::SECUR_RESP, error_code);

	}

	static std::string RESET_REQ(int error_code) {

		return DTEEncoder::encode(DTECommand::RESET_RESP, error_code);

	}

	static std::string FACTR_REQ(int error_code) {

		return DTEEncoder::encode(DTECommand::FACTR_RESP, error_code);

	}

	static std::string DUMPM_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::DUMPM_RESP, error_code);
		}

		unsigned int address = std::get<unsigned int>(arg_list[0]);
		unsigned int length = std::get<unsigned int>(arg_list[1]);
		BaseRawData raw = {
			memory_access->get_physical_address(address, length),
			length
		};

		return DTEEncoder::encode(DTECommand::DUMPM_RESP, error_code, raw);
	}

	static std::string ZONEW_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (!error_code) {
			BaseZone zone;
			std::string zone_bits = std::get<std::string>(arg_list[0]);
			ZoneCodec::decode(zone_bits, zone);
			configuration_store->write_zone(zone);
		}

		return DTEEncoder::encode(DTECommand::ZONEW_RESP, error_code);
	}

	static std::string ZONER_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::ZONER_RESP, error_code);
		}

		unsigned int zone_id = std::get<unsigned int>(arg_list[0]);
		BaseZone& zone = configuration_store->read_zone((uint8_t)zone_id);

		BaseRawData zone_raw;
		zone_raw.length = 0;
		ZoneCodec::encode(zone, zone_raw.str);

		return DTEEncoder::encode(DTECommand::ZONER_RESP, error_code, zone_raw);
	}

public:
	static DTEAction handle_dte_message(std::string& req, std::string& resp) {
		DTECommand command;
		std::vector<ParamID> params;
		std::vector<ParamValue> param_values;
		std::vector<BaseType> arg_list;
		unsigned int error_code = (unsigned int)DTEError::OK;
		DTEAction action = DTEAction::NONE;

		try {
			if (!DTEDecoder::decode(req, command, error_code, arg_list, params, param_values))
				return action;
		} catch (ErrorCode e) {
			switch (e) {
			case ErrorCode::DTE_PROTOCOL_MESSAGE_TOO_LARGE:
			case ErrorCode::DTE_PROTOCOL_PARAM_KEY_UNRECOGNISED:
			case ErrorCode::DTE_PROTOCOL_UNEXPECTED_ARG:
			case ErrorCode::DTE_PROTOCOL_VALUE_OUT_OF_RANGE:
			case ErrorCode::DTE_PROTOCOL_MISSING_ARG:
			case ErrorCode::DTE_PROTOCOL_BAD_FORMAT:
				error_code = (unsigned int)DTEError::INCORRECT_DATA;
				break;
			case ErrorCode::DTE_PROTOCOL_PAYLOAD_LENGTH_MISMATCH:
				error_code = (unsigned int)DTEError::DATA_LENGTH_MISMATCH;
				break;
			case ErrorCode::DTE_PROTOCOL_UNKNOWN_COMMAND:
				error_code = (unsigned int)DTEError::INCORRECT_COMMAND;
				break;
			default:
				throw e; // Error code is unexpected
				break;
			}
		}

		switch(command) {
		case DTECommand::PARML_REQ:
			resp = PARML_REQ(error_code);
			break;
		case DTECommand::PARMW_REQ:
			resp = PARMW_REQ(error_code, param_values);
			break;
		case DTECommand::PARMR_REQ:
			resp = PARMR_REQ(error_code, params);
			break;
		case DTECommand::PROFW_REQ:
			resp = PROFW_REQ(error_code, arg_list);
			break;
		case DTECommand::PROFR_REQ:
			resp = PROFR_REQ(error_code);
			break;
		case DTECommand::SECUR_REQ:
			resp = SECUR_REQ(error_code);
			if (!error_code) action = DTEAction::SECUR;
			break;
		case DTECommand::RESET_REQ:
			resp = RESET_REQ(error_code);
			if (!error_code) action = DTEAction::RESET;
			break;
		case DTECommand::FACTR_REQ:
			resp = FACTR_REQ(error_code);
			if (!error_code) action = DTEAction::FACTR;
			break;
		case DTECommand::DUMPM_REQ:
			resp = DUMPM_REQ(error_code, arg_list);
			break;
		case DTECommand::ZONEW_REQ:
			resp = ZONEW_REQ(error_code, arg_list);
			break;
		case DTECommand::ZONER_REQ:
			resp = ZONER_REQ(error_code, arg_list);
			break;
		default:
			break;
		}

		return action;
	}
};