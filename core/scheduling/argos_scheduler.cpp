
#include <algorithm>
#include <climits>
#include <iomanip>

#include "messages.hpp"
#include "argos_scheduler.hpp"
#include "rtc.hpp"
#include "config_store.hpp"
#include "scheduler.hpp"
#include "bitpack.hpp"
#include "timeutils.hpp"
#include "bch.hpp"
#include "crc8.hpp"

#define INVALID_SCHEDULE    (std::time_t)-1

#define HOURS_PER_DAY       24
#define SECONDS_PER_MINUTE	60
#define SECONDS_PER_HOUR    3600
#define SECONDS_PER_DAY     (SECONDS_PER_HOUR * HOURS_PER_DAY)
#define MM_PER_METER		1000
#define MM_PER_KM   		1000000
#define MV_PER_UNIT			30
#define MS_PER_SEC			1000
#define DEGREES_PER_UNIT	((double)1/1.42)
#define BITS_PER_BYTE		8

#define LON_LAT_RESOLUTION  10000

#define MAX_GPS_ENTRIES_IN_PACKET	4

#define SHORT_PACKET_HEADER_BYTES   7
#define SHORT_PACKET_PAYLOAD_BITS   99
#define SHORT_PACKET_BYTES			22
#define SHORT_PACKET_MSG_LENGTH		6
#define SHORT_PACKET_BITFIELD       0x11

#define LONG_PACKET_HEADER_BYTES    7
#define LONG_PACKET_PAYLOAD_BITS    216
#define LONG_PACKET_BYTES			38
#define LONG_PACKET_MSG_LENGTH		15
#define LONG_PACKET_BITFIELD        0x8B

#define PACKET_SYNC					0xFFFC2F


extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC       *rtc;
extern Logger    *sensor_log;


ArgosScheduler::ArgosScheduler() {
	m_is_running = false;
	m_switch_state = false;
	m_earliest_tx = 0;
}

void ArgosScheduler::reschedule() {
	deschedule();

	std::time_t schedule = INVALID_SCHEDULE;
	if (m_argos_config.mode == BaseArgosMode::OFF) {
		DEBUG_WARN("ArgosScheduler: mode is OFF -- not scheduling");
		return;
	} else if (m_argos_config.mode == BaseArgosMode::LEGACY) {
		// In legacy mode we schedule every hour aligned to UTC
		schedule = next_duty_cycle(0xFFFFFFU);
	} else if (m_argos_config.mode == BaseArgosMode::DUTY_CYCLE) {
		schedule = next_duty_cycle(m_argos_config.duty_cycle);
	} else if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
		schedule = next_prepass();
	}

	if (INVALID_SCHEDULE != schedule) {
		m_argos_task = system_scheduler->post_task_prio(std::bind(&ArgosScheduler::process_schedule, this), Scheduler::DEFAULT_PRIORITY, MS_PER_SEC * schedule);
	} else {
		DEBUG_WARN("ArgosScheduler: no valid schedule found");
	}
}

std::time_t ArgosScheduler::next_duty_cycle(unsigned int duty_cycle)
{
	// Find epoch time for start of the "current" day
	std::time_t now = rtc->gettime();
	std::time_t start_of_day = now - (now % (SECONDS_PER_DAY));
	std::time_t schedule = INVALID_SCHEDULE;
	unsigned int hour = 0;

	// Iteratively find the nearest TR_NOM that is both after current time, earliest TX and within the duty cycle
	// Note that duty cycle is a bit-field comprising 24 bits as follows:
	// 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00  bit
	// 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 21 21 22 23  hour (UTC)
	while (hour < 2*HOURS_PER_DAY) {
		DEBUG_TRACE("ArgosScheduler::next_duty_cycle: hour: %u duty_cycle: %06x now: %u s_o_d: %u", hour, duty_cycle, now, start_of_day);
		// Only select if the TR_NOM entry falls within the duty cycle spec and is also later than earliest allowed TX
		if ((duty_cycle & (0x800000 >> (hour % HOURS_PER_DAY))) && start_of_day >= now && start_of_day >= m_earliest_tx) {
			schedule = start_of_day - now;  // TR_NOM found
			break;
		}
		// Try next TR_NOM
		start_of_day += m_argos_config.tr_nom;
		hour = start_of_day / SECONDS_PER_HOUR;
	}

	DEBUG_TRACE("ArgosScheduler::next_duty_cycle: computed schedule: %d", schedule);

	return schedule;
}

std::time_t ArgosScheduler::next_prepass() {
	DEBUG_WARN("ArgosScheduler: mode PASS_PREDICTION not implemented");
	return 0;
}

void ArgosScheduler::process_schedule() {
	if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
		pass_prediction_algorithm();
	} else {
		periodic_algorithm();
	}
}

void ArgosScheduler::pass_prediction_algorithm() {
	unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
	unsigned int index;
	ArgosPacket packet;

	DEBUG_TRACE("ArgosScheduler::pass_prediction_algorithm: m_msg_index=%u max_index=%u", m_msg_index, max_index);

	// Find first eligible slot for transmission
	while (m_msg_index < (m_msg_index + max_index)) {
		index = m_msg_index % max_index;
		if (m_gps_entries[index].size())
		{
			DEBUG_TRACE("ArgosScheduler::pass_prediction_algorithm: eligible slot=%u", index);
			break;
		}
		m_msg_index++;
	}

	// No further action if no slot is eligible for transmission
	if (m_msg_index == (m_msg_index + max_index)) {
		DEBUG_TRACE("ArgosScheduler::pass_prediction_algorithm: no eligible slot found");
		return;
	}

	if (m_gps_entries[index].size() == 1) {
		build_short_packet(m_gps_entries[index][0], packet);
		handle_packet(packet, SHORT_PACKET_BYTES * BITS_PER_BYTE);
	} else {
		build_long_packet(m_gps_entries[index], packet);
		handle_packet(packet, LONG_PACKET_BYTES * BITS_PER_BYTE);
	}

	configuration_store->increment_tx_counter();
	m_msg_burst_counter[index]--;
	m_msg_index++;

	// Check if message burst count has reached zero and perform clean-up of this slot
	if (m_msg_burst_counter[index] == 0) {
		m_msg_burst_counter[index] = (m_argos_config.ntry_per_message == 0) ? UINT_MAX : m_argos_config.ntry_per_message;
		m_gps_entries[index].clear();
	}
}

void ArgosScheduler::notify_sensor_log_update() {
	DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update");

	if (m_is_running) {
		if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
			unsigned int msg_index;
			unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
			unsigned int span = std::min((unsigned int)MAX_GPS_ENTRIES_IN_PACKET, (unsigned int)m_argos_config.depth_pile);

			DEBUG_TRACE("ArgosScheduler: notify_sensor_log_update: PASS_PREDICTION mode: max_index=%u span=%u", max_index, span);

			// Find the first slot whose vector has less then depth pile entries in it
			for (msg_index = 0; msg_index < max_index; msg_index++) {
				if (m_gps_entries[msg_index].size() < span)
					break;
			}

			// If a slot is found then append most recent sensor log entry to it
			if (msg_index < max_index) {
				GPSLogEntry gps_entry;
				unsigned int idx = sensor_log->num_entries() - 1;  // Most recent entry in log
				sensor_log->read(&gps_entry, idx);
				m_gps_entries[msg_index].push_back(gps_entry);
				DEBUG_TRACE("ArgosScheduler: notify_sensor_log_update: PASS_PREDICTION mode: appending entry=%u to msg_slot=%u", idx, msg_index);
			} else {
				DEBUG_TRACE("ArgosScheduler: notify_sensor_log_update: PASS_PREDICTION mode: no free slots");
			}
		} else {
			// Reset the message burst counters
			for (unsigned int i = 0; i < MAX_MSG_INDEX; i++)
				m_msg_burst_counter[i] = (m_argos_config.ntry_per_message == 0) ? UINT_MAX : m_argos_config.ntry_per_message;
		}
		reschedule();
	}
}

unsigned int ArgosScheduler::convert_latitude(double x) {
	if (x >= 0)
		return x * LON_LAT_RESOLUTION;
	else
		return ((unsigned int)((x - 0.00005) * -LON_LAT_RESOLUTION)) | 1<<20; // -ve: bit 20 is sign
}

unsigned int ArgosScheduler::convert_longitude(double x) {
	if (x >= 0)
		return x * LON_LAT_RESOLUTION;
	else
		return ((unsigned int)((x - 0.00005) * -LON_LAT_RESOLUTION)) | 1<<21; // -ve: bit 21 is sign
}

void ArgosScheduler::build_short_packet(GPSLogEntry const& gps_entry, ArgosPacket& packet) {
	unsigned int base_pos = 0;

	DEBUG_TRACE("ArgosScheduler::build_short_packet");

	// Reserve required number of bytes
	packet.assign(SHORT_PACKET_BYTES, 0);

	// Header bytes
	PACK_BITS(PACKET_SYNC, packet, base_pos, 24);
	PACK_BITS(SHORT_PACKET_MSG_LENGTH, packet, base_pos, 4);
	PACK_BITS(m_argos_config.argos_id>>8, packet, base_pos, 20);
	PACK_BITS(m_argos_config.argos_id, packet, base_pos, 8);

	// Payload bytes
#ifdef ARGOS_USE_CRC8
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)
#else
	PACK_BITS(SHORT_PACKET_BITFIELD, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: bitfield=%u", SHORT_PACKET_BITFIELD);
#endif
	PACK_BITS(gps_entry.day, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: day=%u", gps_entry.day);
	PACK_BITS(gps_entry.hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: hour=%u", gps_entry.hour);
	PACK_BITS(gps_entry.min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: min=%u", gps_entry.min);

	if (gps_entry.valid) {
		PACK_BITS(convert_latitude(gps_entry.lat), packet, base_pos, 21);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: lat=%u (%lf)", convert_latitude(gps_entry.lat), gps_entry.lat);
		PACK_BITS(convert_longitude(gps_entry.lon), packet, base_pos, 22);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: lon=%u (%lf)", convert_longitude(gps_entry.lon), gps_entry.lon);
		double gspeed = (SECONDS_PER_HOUR * gps_entry.gSpeed) / MM_PER_KM;
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: speed=%u", (unsigned int)gspeed);
		PACK_BITS(gps_entry.headMot * DEGREES_PER_UNIT, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: heading=%u", (unsigned int)(gps_entry.headMot * DEGREES_PER_UNIT));
		PACK_BITS(gps_entry.height / MM_PER_METER, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: altitude=%u", (gps_entry.height / MM_PER_METER));
	} else {
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		PACK_BITS(0xFF, packet, base_pos, 8);
		PACK_BITS(0xFF, packet, base_pos, 8);
		PACK_BITS(0xFF, packet, base_pos, 8);
	}

	PACK_BITS(gps_entry.batt_voltage / MV_PER_UNIT, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: voltage=%u", gps_entry.batt_voltage / MV_PER_UNIT);

	// Calculate CRC8
#ifdef ARGOS_USE_CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(SHORT_PACKET_HEADER_BYTES+1), SHORT_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 8*SHORT_PACKET_HEADER_BYTES;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: crc8=%02x", crc8);
#endif

	// BCH code B127_106_3
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B127_106_3,
			sizeof(BCHEncoder::B127_106_3),
			packet.substr(SHORT_PACKET_HEADER_BYTES), SHORT_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: bch=%06x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B127_106_3_CODE_LEN);
}

BaseDeltaTimeLoc ArgosScheduler::delta_time_loc(GPSLogEntry const& a, GPSLogEntry const& b)
{
	std::time_t t_a, t_b, t_diff;

	t_a = convert_epochtime(a.year, a.month, a.day, a.hour, a.min, a.sec);
	t_b = convert_epochtime(b.year, b.month, b.day, b.hour, b.min, b.sec);

	if (t_a >= t_b) {
		t_diff = t_a - t_b;
	} else {
		t_diff = t_b - t_a;
	}

	if (t_diff >= (24 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_24HR;
	} else if (t_diff >= (12 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_12HR;
	} else if (t_diff >= (6 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_6HR;
	} else if (t_diff >= (4 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_4HR;
	} else if (t_diff >= (3 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_3HR;
	} else if (t_diff >= (2 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_2HR;
	} else if (t_diff >= (1 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_1HR;
	} else if (t_diff >= (30 * SECONDS_PER_MINUTE)) {
		return BaseDeltaTimeLoc::DELTA_T_30MIN;
	} else if (t_diff >= (15 * SECONDS_PER_MINUTE)) {
		return BaseDeltaTimeLoc::DELTA_T_15MIN;
	} else {
		return BaseDeltaTimeLoc::DELTA_T_10MIN;
	}
}

void ArgosScheduler::build_long_packet(std::vector<GPSLogEntry> const& gps_entries, ArgosPacket& packet)
{
	unsigned int base_pos = 0;

	DEBUG_TRACE("ArgosScheduler::build_long_packet: gps_entries: %u", gps_entries.size());

	// Must be at least two GPS entries in the vector to use long packet format
	if (gps_entries.size() < 2) {
		DEBUG_ERROR("ArgosScheduler::build_long_packet: requires at least 2 GPS entries in vector (only %u)", gps_entries.size());
		return;
	}

	// Reserve required number of bytes
	packet.assign(LONG_PACKET_BYTES, 0);

	// Header bytes
	PACK_BITS(PACKET_SYNC, packet, base_pos, 24);
	PACK_BITS(LONG_PACKET_MSG_LENGTH, packet, base_pos, 4);
	PACK_BITS(m_argos_config.argos_id>>8, packet, base_pos, 20);
	PACK_BITS(m_argos_config.argos_id, packet, base_pos, 8);

	// Payload bytes
#ifdef ARGOS_USE_CRC8
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)
#else
	PACK_BITS(LONG_PACKET_BITFIELD, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: bitfield=%u", LONG_PACKET_BITFIELD);
#endif
	PACK_BITS(gps_entries[0].day, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: day=%u", gps_entries[0].day);
	PACK_BITS(gps_entries[0].hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: hour=%u", gps_entries[0].hour);
	PACK_BITS(gps_entries[0].min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: min=%u", gps_entries[0].min);

	// First GPS entry
	if (gps_entries[0].valid) {
		PACK_BITS(convert_latitude(gps_entries[0].lat), packet, base_pos, 21);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: lat=%u (%lf)", convert_latitude(gps_entries[0].lat), gps_entries[0].lat);
		PACK_BITS(convert_longitude(gps_entries[0].lon), packet, base_pos, 22);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: lon=%u (%lf)", convert_longitude(gps_entries[0].lon), gps_entries[0].lon);
		double gspeed = (SECONDS_PER_HOUR * gps_entries[0].gSpeed) / MM_PER_KM;
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: speed=%u", (unsigned int)gspeed);
	} else {
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		PACK_BITS(0xFF, packet, base_pos, 8);
	}

	PACK_BITS(gps_entries[0].batt_voltage / MV_PER_UNIT, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: voltage=%u", gps_entries[0].batt_voltage / MV_PER_UNIT);

	// Delta time loc
	PACK_BITS((unsigned int)delta_time_loc(gps_entries[0], gps_entries[1]), packet, base_pos, 4);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: delta_time_loc=%u", (unsigned int)delta_time_loc(gps_entries[0], gps_entries[1]));

	// Subsequent GPS entries
	for (unsigned int i = 1; i < MAX_GPS_ENTRIES_IN_PACKET; i++) {
		DEBUG_TRACE("ArgosScheduler::build_long_packet: gps_valid[%u]=%u", i, gps_entries[i].valid);
		if (gps_entries.size() <= i || 0 == gps_entries[i].valid) {
			PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
			PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		} else {
			PACK_BITS(convert_latitude(gps_entries[i].lat), packet, base_pos, 21);
			DEBUG_TRACE("ArgosScheduler::build_long_packet: lat[%u]=%u (%lf)", i, convert_latitude(gps_entries[i].lat), gps_entries[i].lat);
			PACK_BITS(convert_longitude(gps_entries[i].lon), packet, base_pos, 22);
			DEBUG_TRACE("ArgosScheduler::build_long_packet: lon[%u]=%u (%lf)", i, convert_longitude(gps_entries[i].lon), gps_entries[i].lon);
		}
	}

	// Calculate CRC8
#ifdef ARGOS_USE_CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(LONG_PACKET_HEADER_BYTES+1), LONG_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 8*LONG_PACKET_HEADER_BYTES;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: crc8=%02x", crc8);
#endif

	// BCH code B255_223_4
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B255_223_4,
			sizeof(BCHEncoder::B255_223_4),
			packet.substr(LONG_PACKET_HEADER_BYTES), LONG_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: bch=%08x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B255_223_4_CODE_LEN);
}

void ArgosScheduler::handle_packet(ArgosPacket const& packet, unsigned int total_bits) {
	DEBUG_TRACE("ArgosScheduler::handle_packet: bytes=%u total_bits=%u freq=%lf power=%u",
			packet.size(), total_bits, m_argos_config.frequency, m_argos_config.power);
	DEBUG_TRACE("ArgosScheduler::handle_packet: data=");
#ifdef DEBUG_ENABLE
	for (unsigned int i = 0; i < packet.size(); i++)
		std::cout << std::setfill('0') << std::setw(2) << std::hex << (((unsigned int)packet[i]) & 0xFF);
	std::cout << endl;
#endif
	power_on();
	set_frequency(m_argos_config.frequency);
	set_tx_power(m_argos_config.power);
	send_packet(packet, total_bits);
	power_off();
}

void ArgosScheduler::periodic_algorithm() {
	unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
	unsigned int index = m_msg_index % max_index;
	unsigned int num_entries = sensor_log->num_entries();
	unsigned int span = std::min((unsigned int)MAX_GPS_ENTRIES_IN_PACKET, (unsigned int)m_argos_config.depth_pile);
	ArgosPacket packet;
	GPSLogEntry gps_entry;

	DEBUG_TRACE("ArgosScheduler::periodic_algorithm: msg_index=%u burst_counter=%u span=%u num_log_entries=%u", m_msg_index, m_msg_burst_counter[index], span, num_entries);

	if (m_msg_burst_counter[index] == 0) {
		DEBUG_WARN("ArgosScheduler::periodic_algorithm: burst counter is zero; not transmitting");
		return;
	}

	if (num_entries < (span * (index+1))) {
		DEBUG_WARN("ArgosScheduler::periodic_algorithm: insufficient entries in sensor log; not transmitting");
		return;
	}

	if (span == 1) {
		// Read the GPS entry according to its index
		unsigned int idx = num_entries - index - 1;
		DEBUG_TRACE("read gps_entry=%u", idx);
		sensor_log->read(&gps_entry, idx);
		build_short_packet(gps_entry, packet);
		handle_packet(packet, SHORT_PACKET_BYTES * BITS_PER_BYTE);
	} else {
		std::vector<GPSLogEntry> gps_entries;
		for (unsigned int k = 0; k < span; k++) {
			unsigned int idx = num_entries - (span * (index+1)) + k;
			DEBUG_TRACE("read gps_entry=%u", idx);
			sensor_log->read(&gps_entry, idx);
			gps_entries.push_back(gps_entry);
		}
		build_long_packet(gps_entries, packet);
		handle_packet(packet, LONG_PACKET_BYTES * BITS_PER_BYTE);
	}

	configuration_store->increment_tx_counter();
	m_msg_burst_counter[index]--;
	m_msg_index++;
}

void ArgosScheduler::start() {
	m_is_running = true;
	m_msg_index = 0;
	configuration_store->get_argos_configuration(m_argos_config);
	for (unsigned int i = 0; i < MAX_MSG_INDEX; i++) {
		m_msg_burst_counter[i] = (m_argos_config.ntry_per_message == 0) ? UINT_MAX : m_argos_config.ntry_per_message;
		m_gps_entries[i].clear();
	}
}

void ArgosScheduler::stop() {
	deschedule();
	m_is_running = false;
}

void ArgosScheduler::deschedule() {
	if (system_scheduler->is_scheduled(m_argos_task)) {
		system_scheduler->cancel_task(m_argos_task);
	}
}

void ArgosScheduler::notify_saltwater_switch_state(bool state) {
	m_switch_state = state;
	if (m_is_running && m_argos_config.underwater_en) {
		if (!m_switch_state) {
			m_earliest_tx = rtc->gettime() + m_argos_config.dry_time_before_tx;
			reschedule();
		} else {
			deschedule();
		}
	}
}