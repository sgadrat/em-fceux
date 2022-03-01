#include "rainbow_esp.h"

#include "../fceu.h"
#include "../utils/crc32.h"

#include <emscripten.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <map>
#include <sstream>

#define RAINBOW_DEBUG 0

#if RAINBOW_DEBUG >= 1
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

#if RAINBOW_DEBUG >= 2
#define UDBG_FLOOD(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG_FLOOD(...)
#endif

#if RAINBOW_DEBUG >= 1
#include "../debug.h"
namespace {
uint64_t wall_clock_milli() {
};
}
#endif

namespace {

std::array<std::string, NUM_FILE_PATHS> dir_names = { "SAVE", "ROMS", "USER" };

}

BrokeStudioFirmware::BrokeStudioFirmware() {
	UDBG("RAINBOW BrokeStudioFirmware ctor\n");

	// Get default host/port
	char const* hostname = ::getenv("RAINBOW_SERVER_ADDR");
	if (hostname == nullptr) hostname = "";
	this->server_settings_address = hostname;
	this->default_server_settings_address = hostname;

	char const* port_cstr = ::getenv("RAINBOW_SERVER_PORT");
	if (port_cstr == nullptr) port_cstr = "0";
	std::istringstream port_iss(port_cstr);
	port_iss >> this->server_settings_port;
	this->default_server_settings_port = this->server_settings_port;

	// Clear file list
	for (uint8 p = 0; p < NUM_FILE_PATHS; p++)
	{
		for (uint8 f = 0; f < NUM_FILES; f++)
		{
			this->file_exists[p][f] = false;
		}
	}

	// Init fake registered networks
	this->networks = { {
		{"FCEUX_SSID", "FCEUX_PASS"},
		{"", ""},
		{"", ""},
	} };

	// Load file list from save file (if any)
	this->loadFiles();
}

BrokeStudioFirmware::~BrokeStudioFirmware() {
	UDBG("RAINBOW BrokeStudioFirmware dtor\n");

	this->closeConnection();
}

void BrokeStudioFirmware::rx(uint8 v) {
	UDBG_FLOOD("RAINBOW BrokeStudioFirmware rx %02x\n", v);
	if (this->msg_first_byte) {
		this->msg_first_byte = false;
		this->msg_length = v + 1;
	}
	this->rx_buffer.push_back(v);

	if (this->rx_buffer.size() == this->msg_length) {
		this->processBufferedMessage();
		this->msg_first_byte = true;
	}
}

uint8 BrokeStudioFirmware::tx() {
	// Refresh buffer from network
	this->receiveDataFromServer();
	this->receivePingResult();

	// Fill buffer with the next message (if needed)
	if (this->tx_buffer.empty() && !this->tx_messages.empty()) {
		std::deque<uint8> message = this->tx_messages.front();
		this->tx_buffer.insert(this->tx_buffer.end(), message.begin(), message.end());
		this->tx_messages.pop_front();
	}

	// Get byte from buffer
	if (!this->tx_buffer.empty()) {
		last_byte_read = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	UDBG_FLOOD("RAINBOW BrokeStudioFirmware tx %02x\n", last_byte_read);
	return last_byte_read;
}

bool BrokeStudioFirmware::getDataReadyIO() {
	this->receiveDataFromServer();
	this->receivePingResult();
	return !(this->tx_buffer.empty() && this->tx_messages.empty());
}

void BrokeStudioFirmware::processBufferedMessage() {
	assert(this->rx_buffer.size() >= 2); // Buffer must contain exactly one message, minimal message is two bytes (length + type)
	uint8 const message_size = this->rx_buffer.front();
	assert(message_size >= 1); // minimal payload is one byte (type)
	assert(this->rx_buffer.size() == static_cast<std::deque<uint8>::size_type>(message_size) + 1); // Buffer size must match declared payload size

	// Process the message in RX buffer
	switch (static_cast<toesp_cmds_t>(this->rx_buffer.at(1))) {
		
		// ESP CMDS

		case toesp_cmds_t::ESP_GET_STATUS:
			UDBG("RAINBOW BrokeStudioFirmware received message ESP_GET_STATUS\n");
			this->tx_messages.push_back({1, static_cast<uint8>(fromesp_cmds_t::READY)});
			break;
		case toesp_cmds_t::DEBUG_GET_LEVEL:
			UDBG("RAINBOW BrokeStudioFirmware received message DEBUG_GET_LEVEL\n");
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(fromesp_cmds_t::DEBUG_LEVEL),
				static_cast<uint8>(this->debug_config)
			});
			break;
		case toesp_cmds_t::DEBUG_SET_LEVEL:
			UDBG("RAINBOW BrokeStudioFirmware received message DEBUG_SET_LEVEL\n");
			if (message_size == 3) {
				this->debug_config = this->rx_buffer.at(2);
			}
			FCEU_printf("DEBUG LEVEL SET TO: %u\n", this->debug_config);
			break;
		case toesp_cmds_t::DEBUG_LOG:
			UDBG("RAINBOW DEBUG/LOG\n");
			if (RAINBOW_DEBUG > 0 || (this->debug_config & 1)) {
				for (std::deque<uint8>::const_iterator cur = this->rx_buffer.begin() + 2; cur < this->rx_buffer.end(); ++cur) {
					FCEU_printf("%02x ", *cur);
				}
				FCEU_printf("\n");
			}
			break;
		case toesp_cmds_t::BUFFER_CLEAR_RX_TX:
			UDBG("RAINBOW BrokeStudioFirmware received message BUFFER_CLEAR_RX_TX\n");
			this->receiveDataFromServer();
			this->receivePingResult();
			this->tx_buffer.clear();
			this->tx_messages.clear();
			this->rx_buffer.clear();
			break;
		case toesp_cmds_t::BUFFER_DROP_FROM_ESP:
			UDBG("RAINBOW BrokeStudioFirmware received message BUFFER_DROP_FROM_ESP\n");
			if (message_size == 3) {
				uint8 const message_type = this->rx_buffer.at(2);
				uint8 const n_keep = this->rx_buffer.at(3);

				size_t i = 0;
				for (
					std::deque<std::deque<uint8>>::iterator message = this->tx_messages.end();
					message != this->tx_messages.begin();
				)
				{
					--message;
					if (message->at(1) == message_type) {
						++i;
						if (i > n_keep) {
							UDBG("RAINBOW BrokeStudioFirmware erase message: index=%d\n", message - this->tx_messages.begin());
							message = this->tx_messages.erase(message);
						}else {
							UDBG("RAINBOW BrokeStudioFirmware keep message: index=%d - too recent\n", message - this->tx_messages.begin());
						}
					}else {
						UDBG("RAINBOW BrokeStudioFirmware keep message: index=%d - bad type\n", message - this->tx_messages.begin());
					}
				}
			}
			break;
		case toesp_cmds_t::ESP_GET_FIRMWARE_VERSION:
			UDBG("RAINBOW BrokeStudioFirmware received message ESP_GET_FIRMWARE_VERSION\n");
			this->tx_messages.push_back({ 7, static_cast<uint8>(fromesp_cmds_t::ESP_FIRMWARE_VERSION), 5, 'F', 'C', 'E', 'U', 'X' });
			break;


		// WIFI CMDS
		case toesp_cmds_t::WIFI_GET_STATUS:
			UDBG("RAINBOW BrokeStudioFirmware received message WIFI_GET_STATUS\n");
			this->tx_messages.push_back({ 2, static_cast<uint8>(fromesp_cmds_t::WIFI_STATUS), 3 }); // Simple answer, wifi is ok
			break;
		case toesp_cmds_t::WIFI_GET_SSID:
			UDBG("RAINBOW BrokeStudioFirmware received message WIFI_GET_SSID\n");
			this->tx_messages.push_back({ 12, static_cast<uint8>(fromesp_cmds_t::SSID), 10, 'F', 'C', 'E', 'U', 'X', '_', 'S', 'S', 'I', 'D' });
			break;
		case toesp_cmds_t::WIFI_GET_IP:
			UDBG("RAINBOW BrokeStudioFirmware received message WIFI_GET_ID\n");
			this->tx_messages.push_back({ 14, static_cast<uint8>(fromesp_cmds_t::IP_ADDRESS), 12, '1', '9', '2', '.', '1', '6', '8', '.', '1', '.', '1', '0' });
			break;

		// AP CMDS
		// GET/SET AP config commands are not relevant here, so we'll just use a fake variable
		case toesp_cmds_t::AP_GET_SSID:
			UDBG("RAINBOW BrokeStudioFirmware received message AP_GET_SSID\n");
			this->tx_messages.push_back({ 12, static_cast<uint8>(fromesp_cmds_t::SSID), 10, 'F', 'C', 'E', 'U', 'X', '_', 'S', 'S', 'I', 'D' });
			break;
		case toesp_cmds_t::AP_GET_IP:
			UDBG("RAINBOW BrokeStudioFirmware received message AP_GET_ID\n");
			this->tx_messages.push_back({ 16, static_cast<uint8>(fromesp_cmds_t::IP_ADDRESS), 14, '1', '2', '7', '.', '0', '.', '0', '.', '1', ':', '8', '0', '8', '0' });
			break;
		case toesp_cmds_t::AP_GET_CONFIG:
			UDBG("RAINBOW BrokeStudioFirmware received message AP_GET_CONFIG\n");
			this->tx_messages.push_back({ 2, static_cast<uint8>(fromesp_cmds_t::AP_CONFIG), this->ap_config });
			break;
		case toesp_cmds_t::AP_SET_CONFIG:
			UDBG("RAINBOW BrokeStudioFirmware received message AP_SET_CONFIG\n");
			this->ap_config = this->rx_buffer.at(2);
			break;

		// RND CMDS

		case toesp_cmds_t::RND_GET_BYTE:
			UDBG("RAINBOW BrokeStudioFirmware received message RND_GET_BYTE\n");
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(fromesp_cmds_t::RND_BYTE),
				static_cast<uint8>(rand() % 256)
			});
			break;
		case toesp_cmds_t::RND_GET_BYTE_RANGE: {
			UDBG("RAINBOW BrokeStudioFirmware received message RND_GET_BYTE_RANGE\n");
			if (message_size < 3) {
				break;
			}
			int const min_value = this->rx_buffer.at(2);
			int const max_value = this->rx_buffer.at(3);
			int const range = max_value - min_value;
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(fromesp_cmds_t::RND_BYTE),
				static_cast<uint8>(min_value + (rand() % range))
			});
			break;
		}
		case toesp_cmds_t::RND_GET_WORD:
			UDBG("RAINBOW BrokeStudioFirmware received message RND_GET_WORD\n");
			this->tx_messages.push_back({
				3,
				static_cast<uint8>(fromesp_cmds_t::RND_WORD),
				static_cast<uint8>(rand() % 256),
				static_cast<uint8>(rand() % 256)
			});
			break;
		case toesp_cmds_t::RND_GET_WORD_RANGE: {
			UDBG("RAINBOW BrokeStudioFirmware received message RND_GET_WORD_RANGE\n");
			if (message_size < 5) {
				break;
			}
			int const min_value = (static_cast<int>(this->rx_buffer.at(2)) << 8) + this->rx_buffer.at(3);
			int const max_value = (static_cast<int>(this->rx_buffer.at(4)) << 8) + this->rx_buffer.at(5);
			int const range = max_value - min_value;
			int const rand_value = min_value + (rand() % range);
			this->tx_messages.push_back({
				3,
				static_cast<uint8>(fromesp_cmds_t::RND_WORD),
				static_cast<uint8>(rand_value >> 8),
				static_cast<uint8>(rand_value & 0xff)
			});
			break;
		}

		// SERVER CMDS

		case toesp_cmds_t::SERVER_GET_STATUS: {
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_GET_STATUS\n");
			uint8 status;
			switch (this->active_protocol) {
			case server_protocol_t::WEBSOCKET:
			case server_protocol_t::WEBSOCKET_SECURED:
				status = 0; //TODO
				break;
			case server_protocol_t::TCP:
			case server_protocol_t::TCP_SECURED:
				status = 0; //TODO
			case server_protocol_t::UDP:
				status = 0; //TODO
				break;
			default:
				status = 0; // Unknown active protocol, connection certainly broken
			}

			this->tx_messages.push_back({
				2,
				static_cast<uint8>(fromesp_cmds_t::SERVER_STATUS),
				status
			});
			break;
		}
		case toesp_cmds_t::SERVER_PING:
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_PING\n");
			if (!this->ping_running) {
				if (this->server_settings_address.empty()) {
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(fromesp_cmds_t::SERVER_PING)
					});
				}else if (message_size >= 1) {
					this->ping_running = true;
					uint8 n = (message_size == 1 ? 0 : this->rx_buffer.at(2));
					if (n == 0) {
						n = 4;
					}
					EM_ASM({
						FCEM.pingServer(UTF8ToString($0), $1, $2);
					}, this->server_settings_address.c_str(), this->server_settings_port, n);
				}
			}
			break;
		case toesp_cmds_t::SERVER_SET_PROTOCOL: {
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_SET_PROTOCOL\n");
			if (message_size == 2) {
				server_protocol_t const requested_protocol = static_cast<server_protocol_t>(this->rx_buffer.at(2));
				if (requested_protocol > server_protocol_t::UDP) {
					UDBG("RAINBOW BrokeStudioFirmware SET_SERVER_PROTOCOL: unknown protocol (%d)\n", requested_protocol);
				}else {
					this->active_protocol = requested_protocol;
				}
			}
			break;
		}
		case toesp_cmds_t::SERVER_GET_SETTINGS: {
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_GET_SETTINGS\n");
			if (this->server_settings_address.empty() && this->server_settings_port == 0) {
				this->tx_messages.push_back({
					1,
					static_cast<uint8>(fromesp_cmds_t::SERVER_SETTINGS)
				});
			}else {
				std::deque<uint8> message({
					static_cast<uint8>(1 + 2 + this->server_settings_address.size()),
					static_cast<uint8>(fromesp_cmds_t::SERVER_SETTINGS),
					static_cast<uint8>(this->server_settings_port >> 8),
					static_cast<uint8>(this->server_settings_port & 0xff)
				});
				message.insert(message.end(), this->server_settings_address.begin(), this->server_settings_address.end());
				this->tx_messages.push_back(message);
			}
			break;
		}
		case toesp_cmds_t::SERVER_GET_CONFIG_SETTINGS: {
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_GET_CONFIG_SETTINGS\n");
			if (this->default_server_settings_address.empty() && this->default_server_settings_port == 0) {
				this->tx_messages.push_back({
					1,
					static_cast<uint8>(fromesp_cmds_t::SERVER_SETTINGS)
				});
			}else {
				std::deque<uint8> message({
					static_cast<uint8>(1 + 2 + this->default_server_settings_address.size()),
					static_cast<uint8>(fromesp_cmds_t::SERVER_SETTINGS),
					static_cast<uint8>(this->default_server_settings_port >> 8),
					static_cast<uint8>(this->default_server_settings_port & 0xff)
				});
				message.insert(message.end(), this->default_server_settings_address.begin(), this->default_server_settings_address.end());
				this->tx_messages.push_back(message);
			}
			break;
		}
		case toesp_cmds_t::SERVER_SET_SETTINGS:
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_SET_SETTINGS\n");
			if (message_size >= 3) {
				this->server_settings_port =
					(static_cast<uint16_t>(this->rx_buffer.at(2)) << 8) +
					(static_cast<uint16_t>(this->rx_buffer.at(3)))
				;
				this->server_settings_address = std::string(this->rx_buffer.begin()+4, this->rx_buffer.end());
			}
			break;
		case toesp_cmds_t::SERVER_RESTORE_SETTINGS:
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_RESTORE_SETTINGS\n");
			this->server_settings_address = this->default_server_settings_address;
			this->server_settings_port = this->default_server_settings_port;
			break;
		case toesp_cmds_t::SERVER_CONNECT:
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_CONNECT\n");
			this->openConnection();
			break;
		case toesp_cmds_t::SERVER_DISCONNECT:
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_DISCONNECT\n");
			this->closeConnection();
			break;
		case toesp_cmds_t::SERVER_SEND_MSG: {
			UDBG("RAINBOW BrokeStudioFirmware received message SERVER_SEND_MSG\n");
			uint8 const payload_size = this->rx_buffer.size() - 2;
			std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
			std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;

			switch (this->active_protocol) {
			case server_protocol_t::WEBSOCKET:
			case server_protocol_t::WEBSOCKET_SECURED:
				this->sendMessageToServer(payload_begin, payload_end);
				break;
			case server_protocol_t::UDP:
				this->sendUdpDatagramToServer(payload_begin, payload_end);
				break;
			default:
				UDBG("RAINBOW BrokeStudioFirmware active protocol (%d) not implemented\n", this->active_protocol);
			};
			break;
		}

		// NETWORK CMDS
		// network commands are not relevant here, so we'll just use test/fake data
		case toesp_cmds_t::NETWORK_SCAN:
			UDBG("RAINBOW BrokeStudioFirmware received message NETWORK_SCAN\n");
			if (message_size == 1) {
				this->tx_messages.push_back({
					2,
					static_cast<uint8>(fromesp_cmds_t::NETWORK_COUNT),
					NUM_FAKE_NETWORKS
				});
			}
			break;
		case toesp_cmds_t::NETWORK_GET_DETAILS:
			UDBG("RAINBOW BrokeStudioFirmware received message NETWORK_GET_DETAILS\n");
			if (message_size == 2) {
				uint8 networkItem = this->rx_buffer.at(2);
				if (networkItem > NUM_FAKE_NETWORKS-1) networkItem = NUM_FAKE_NETWORKS-1;
				this->tx_messages.push_back({
					21,
					static_cast<uint8>(fromesp_cmds_t::NETWORK_SCANNED_DETAILS),
					4, // encryption type
					0x47, // RSSI
					0x00,0x00,0x00,0x01, // channel
					0, // hidden?
					12, // SSID length
					'F','C','E','U','X','_','S','S','I','D','_',static_cast<uint8>(networkItem + '0') // SSID
				});
			}
			break;
		case toesp_cmds_t::NETWORK_GET_REGISTERED:
			UDBG("RAINBOW BrokeStudioFirmware received message NETWORK_GET_REGISTERED\n");
			if (message_size == 1) {
				this->tx_messages.push_back({
					NUM_NETWORKS+1,
					static_cast<uint8>(fromesp_cmds_t::NETWORK_REGISTERED),
					static_cast<uint8>((this->networks[0].ssid != "") ? 1 : 0),
					static_cast<uint8>((this->networks[1].ssid != "") ? 1 : 0),
					static_cast<uint8>((this->networks[2].ssid != "") ? 1 : 0)
				});
			}
			break;
		case toesp_cmds_t::NETWORK_GET_REGISTERED_DETAILS:
			UDBG("RAINBOW BrokeStudioFirmware received message NETWORK_GET_REGISTERED_DETAILS\n");
			if (message_size == 2) {
				uint8 networkItem = this->rx_buffer.at(2);
				if (networkItem > NUM_NETWORKS - 1) networkItem = NUM_NETWORKS - 1;
				std::deque<uint8> message({
					static_cast<uint8>(2 + this->networks[networkItem].ssid.length() + 1 + this->networks[networkItem].pass.length()),
					static_cast<uint8>(fromesp_cmds_t::NETWORK_REGISTERED_DETAILS),
					static_cast<uint8>(this->networks[networkItem].ssid.length())
				});
				message.insert(message.end(), this->networks[networkItem].ssid.begin(), this->networks[networkItem].ssid.end());
				message.insert(message.end(), this->networks[networkItem].pass.length());
				message.insert(message.end(), this->networks[networkItem].pass.begin(), this->networks[networkItem].pass.end());
				this->tx_messages.push_back(message);
			}
			break;
		case toesp_cmds_t::NETWORK_REGISTER:
			UDBG("RAINBOW BrokeStudioFirmware received message NETWORK_REGISTER\n");
			if (message_size > 6) {
				uint8 const networkItem = this->rx_buffer.at(2);
				if (networkItem > NUM_NETWORKS - 1) break;
				uint8 SSIDlength = std::min(SSID_MAX_LENGTH, this->rx_buffer.at(3));
				uint8 PASSlength = std::min(PASS_MAX_LENGTH, this->rx_buffer.at(4 + SSIDlength));
				this->networks[networkItem].ssid = std::string(this->rx_buffer.begin() + 4, this->rx_buffer.begin() + 4 + SSIDlength);
				this->networks[networkItem].pass = std::string(this->rx_buffer.begin() + 4 + SSIDlength + 1, this->rx_buffer.begin() + 4 + SSIDlength + 1 + PASSlength);

			}
			break;
		case toesp_cmds_t::NETWORK_UNREGISTER:
			UDBG("RAINBOW BrokeStudioFirmware received message NETWORK_UNREGISTER\n");
			if (message_size == 2) {
				uint8 const networkItem = this->rx_buffer.at(2);
				if (networkItem > NUM_NETWORKS - 1) break;
				this->networks[networkItem].ssid = "";
				this->networks[networkItem].pass = "";
			}
			break;

		// FILE CMDS

		case toesp_cmds_t::FILE_OPEN: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_OPEN\n");
			if (message_size >= 4) {
				uint8 config = this->rx_buffer.at(2);
				uint8 access_mode = config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

				if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
					uint8 const path = this->rx_buffer.at(3);
					uint8 const file = this->rx_buffer.at(4);
					if (path < NUM_FILE_PATHS && file < NUM_FILES) {
						this->file_exists[path][file] = true;
						this->working_path = path;
						this->working_file = file;
						this->working_file_config = config;
						this->file_offset = 0;
						this->saveFiles();
					}
				}else {
					//TODO manual mode
				}
			}
			break;
		}
		case toesp_cmds_t::FILE_CLOSE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_CLOSE\n");
			this->working_file = NO_WORKING_FILE;
			this->saveFiles();
			break;
		case toesp_cmds_t::FILE_STATUS: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_STATUS\n");

			uint8 access_mode = this->working_file_config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

			if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
				if (this->working_file == NO_WORKING_FILE) {
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(fromesp_cmds_t::FILE_STATUS),
						0
					});
				}else {
					this->tx_messages.push_back({
						5,
						static_cast<uint8>(fromesp_cmds_t::FILE_STATUS),
						1,
						static_cast<uint8>(this->working_file_config),
						static_cast<uint8>(this->working_path),
						static_cast<uint8>(this->working_file),
					});
				}
			}
			else {
				//TODO manual mode
			}
			break;
		}
		case toesp_cmds_t::FILE_EXISTS: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_EXISTS\n");

			if (message_size < 2) {
				break;
			}
			uint8 config = this->rx_buffer.at(2);
			uint8 access_mode = config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

			if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
				if (message_size == 4) {
					uint8 const path = this->rx_buffer.at(3);
					uint8 const file = this->rx_buffer.at(4);
					if (path < NUM_FILE_PATHS && file < NUM_FILES) {
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(fromesp_cmds_t::FILE_EXISTS),
							static_cast<uint8>(this->file_exists[path][file] ? 1 : 0)
						});
					}
				}
			}else {
				//TODO manual mode
			}
			break;
		}
		case toesp_cmds_t::FILE_DELETE: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_DELETE\n");

			if (message_size < 2) {
				break;
			}
			uint8 config = this->rx_buffer.at(2);
			uint8 access_mode = config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

			if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
				if (message_size == 4) {
					uint8 const path = this->rx_buffer.at(3);
					uint8 const file = this->rx_buffer.at(4);
					if (path < NUM_FILE_PATHS && file < NUM_FILES) {
						if (this->file_exists[path][file]) {
							// File exists, let's delete it
							this->files[path][file].clear();
							this->file_exists[path][file] = false;
							this->tx_messages.push_back({
								2,
								static_cast<uint8>(fromesp_cmds_t::FILE_DELETE),
								static_cast<uint8>(file_delete_results_t::SUCCESS)
							});
							this->saveFiles();
						}else {
							// File does not exist
							this->tx_messages.push_back({
								2,
								static_cast<uint8>(fromesp_cmds_t::FILE_DELETE),
								static_cast<uint8>(file_delete_results_t::FILE_NOT_FOUND)
							});
						}
					}else {
						// Error while deleting the file
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(fromesp_cmds_t::FILE_DELETE),
							static_cast<uint8>(file_delete_results_t::INVALID_PATH_OR_FILE)
						});
					}
				}
			}
			else {
				//TODO manual mode
			}
			break;
		}
		case toesp_cmds_t::FILE_SET_CUR:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_SET_CUR\n");
			if (2 <= message_size && message_size <= 5) {
				this->file_offset = this->rx_buffer.at(2);
				this->file_offset += static_cast<uint32>(message_size >= 3 ? this->rx_buffer.at(3) : 0) << 8;
				this->file_offset += static_cast<uint32>(message_size >= 4 ? this->rx_buffer.at(4) : 0) << 16;
				this->file_offset += static_cast<uint32>(message_size >= 5 ? this->rx_buffer.at(5) : 0) << 24;
			}
			break;
		case toesp_cmds_t::FILE_READ:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_READ\n");
			if (message_size == 2) {
				if (this->working_file != NO_WORKING_FILE) {
					uint8 const n = this->rx_buffer.at(2);
					this->readFile(this->working_path, this->working_file, n, this->file_offset);
					this->file_offset += n;
					UDBG("working file offset: %u (%x)\n", this->file_offset, this->file_offset);
					UDBG("file size: %lu bytes\n", this->files[this->working_path][this->working_file].size());
					if (this->file_offset > this->files[this->working_path][this->working_file].size()) {
						this->file_offset = this->files[this->working_path][this->working_file].size();
					}
				}else {
					this->tx_messages.push_back({2, static_cast<uint8>(fromesp_cmds_t::FILE_DATA), 0});
				}
			}
			break;
		case toesp_cmds_t::FILE_WRITE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_WRITE\n");
			if (message_size >= 3 && this->working_file != NO_WORKING_FILE) {
				this->writeFile(this->working_path, this->working_file, this->file_offset, this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
				this->file_offset += message_size - 1;
			}
			break;
		case toesp_cmds_t::FILE_APPEND:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_APPEND\n");
			if (message_size >= 3 && this->working_file != NO_WORKING_FILE) {
				this->writeFile(this->working_path, this->working_file, this->files[working_path][working_file].size(), this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
			}
			break;
		case toesp_cmds_t::FILE_COUNT: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_COUNT\n");

			if (message_size < 2) {
				break;
			}
			uint8 config = this->rx_buffer.at(2);
			uint8 access_mode = config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

			if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
				if (message_size == 3) {
					uint8 const path = this->rx_buffer.at(3);
					if (path >= NUM_FILE_PATHS) {
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(fromesp_cmds_t::FILE_COUNT),
							0
						});
					}else {
						uint8 nb_files = 0;
						for (bool exists : this->file_exists[path]) {
							if (exists) {
								++nb_files;
							}
						}
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(fromesp_cmds_t::FILE_COUNT),
							nb_files
							});
						UDBG("%u files found in path %u\n", nb_files, path);
					}
				}
			}else {
				//TODO manual mode
			}

			break;
		}case toesp_cmds_t::FILE_GET_LIST: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_LIST\n");

			if (message_size < 2) {
				break;
			}
			uint8 config = this->rx_buffer.at(2);
			uint8 access_mode = config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

			if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
				if (message_size >= 3) {
					std::vector<uint8> existing_files;
					uint8 const path = this->rx_buffer.at(3);
					uint8 page_size = NUM_FILES;
					uint8 current_page = 0;
					if (message_size == 5) {
						page_size = this->rx_buffer.at(4);
						current_page = this->rx_buffer.at(5);
					}
					uint8 page_start = current_page * page_size;
					uint8 page_end = current_page * page_size + page_size;
					uint8 nFiles = 0;
					if (page_end > this->file_exists[path].size())
						page_end = this->file_exists[path].size();
					for (uint8 i = 0; i < NUM_FILES; ++i) {
						if (this->file_exists[path][i]) {
							if (nFiles >= page_start && nFiles < page_end) {
								existing_files.push_back(i);
							}
							nFiles++;
						}
						if (nFiles >= page_end) break;
					}
					std::deque<uint8> message({
						static_cast<uint8>(existing_files.size() + 2),
						static_cast<uint8>(fromesp_cmds_t::FILE_LIST),
						static_cast<uint8>(existing_files.size())
						});
					message.insert(message.end(), existing_files.begin(), existing_files.end());
					this->tx_messages.push_back(message);
				}
			}
			else {
				//TODO manual mode
			}
			break;
		}
		case toesp_cmds_t::FILE_GET_FREE_ID:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_FREE_ID\n");
			if (message_size == 2) {
				uint8 const file_id = this->getFreeFileId(this->rx_buffer.at(2));
				if (file_id != 128) {
					// Free file ID found
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(fromesp_cmds_t::FILE_ID),
						file_id,
					});
				}else {
					// Free file ID not found
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(fromesp_cmds_t::FILE_ID)
					});
				}
			}
			break;
		case toesp_cmds_t::FILE_GET_INFO: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_INFO\n");

			if (message_size < 2) {
				break;
			}
			uint8 config = this->rx_buffer.at(2);
			uint8 access_mode = config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

			if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
				if (message_size == 4) {
					uint8 const path = this->rx_buffer.at(3);
					uint8 const file = this->rx_buffer.at(4);
					if (path < NUM_FILE_PATHS && file < NUM_FILES && this->file_exists[path][file]) {
						// Compute info
						uint32 file_crc32;
						file_crc32 = CalcCRC32(0L, this->files[path][file].data(), this->files[path][file].size());

						uint32 file_size = this->files[path][file].size();

						// Send info
						this->tx_messages.push_back({
							9,
							static_cast<uint8>(fromesp_cmds_t::FILE_INFO),

							static_cast<uint8>((file_crc32 >> 24) & 0xff),
							static_cast<uint8>((file_crc32 >> 16) & 0xff),
							static_cast<uint8>((file_crc32 >> 8) & 0xff),
							static_cast<uint8>(file_crc32 & 0xff),

							static_cast<uint8>((file_size >> 24) & 0xff),
							static_cast<uint8>((file_size >> 16) & 0xff),
							static_cast<uint8>((file_size >> 8) & 0xff),
							static_cast<uint8>(file_size & 0xff)
						});
					}else {
						// File not found or path/file out of bounds
						this->tx_messages.push_back({
							1,
							static_cast<uint8>(fromesp_cmds_t::FILE_INFO)
						});
					}
				}
			}else {
				//TODO manual mode
			}

			break;
		}
		case toesp_cmds_t::FILE_DOWNLOAD: {
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_DOWNLOAD\n");

			if (message_size < 2) {
				break;
			}
			uint8 config = this->rx_buffer.at(2);
			uint8 access_mode = config & static_cast<uint8>(file_config_flags_t::ACCESS_MODE);

			if (access_mode == static_cast<uint8>(file_config_flags_t::AUTO_ACCESS_MODE)) {
				if (message_size > 6) {
					// Parse
					uint8 const urlLength = this->rx_buffer.at(3);
					if (message_size != urlLength + 5) {
						break;
					}
					std::string const url(this->rx_buffer.begin() + 4, this->rx_buffer.begin() + 4 + urlLength);

					uint8 const path = this->rx_buffer.at(4 + urlLength);
					uint8 const file = this->rx_buffer.at(4 + 1 + urlLength);

					// Delete existing file
					if (path < NUM_FILE_PATHS && file < NUM_FILES) {
						if (this->file_exists[path][file]) {
							// File exists, let's delete it
							this->files[path][file].clear();
							this->file_exists[path][file] = false;
							this->saveFiles();
						}
					}else {
						// Invalid path / file
						this->tx_messages.push_back({
							4,
							static_cast<uint8>(fromesp_cmds_t::FILE_DOWNLOAD),
							static_cast<uint8>(file_download_results_t::INVALID_DESTINATION),
							0,
							0
						});
						break;
					}

					// Download new file
					this->downloadFile(url, path, file);
				}
			}else {
				//TODO manual mode
			}
			break;
		}
		case toesp_cmds_t::FILE_FORMAT:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_FORMAT\n");
			if (message_size == 1) {
				// Clear file list
				for (uint8 p = 0; p < NUM_FILE_PATHS; p++) {
					for (uint8 f = 0; f < NUM_FILES; f++) {
						this->file_exists[p][f] = false;
					}
				}
			}
			break;








		default:
			UDBG("RAINBOW BrokeStudioFirmware received unknown message %02x\n", this->rx_buffer.at(1));
			break;
	};

	// Remove processed message
	this->rx_buffer.clear();
}

void BrokeStudioFirmware::readFile(uint8 path, uint8 file, uint8 n, uint32 offset) {
	assert(path < NUM_FILE_PATHS);
	assert(file < NUM_FILES);

	// Get data range
	std::vector<uint8> const& f = this->files[path][file];
	std::vector<uint8>::const_iterator data_begin;
	std::vector<uint8>::const_iterator data_end;
	if (offset >= f.size()) {
		data_begin = f.end();
		data_end = data_begin;
	}else {
		data_begin = f.begin() + offset;
		data_end = f.begin() + std::min(static_cast<std::vector<uint8>::size_type>(offset) + n, f.size());
	}
	std::vector<uint8>::size_type const data_size = data_end - data_begin;

	// Write response
	std::deque<uint8> message({
		static_cast<uint8>(data_size + 2),
		static_cast<uint8>(fromesp_cmds_t::FILE_DATA),
		static_cast<uint8>(data_size)
	});
	message.insert(message.end(), data_begin, data_end);
	this->tx_messages.push_back(message);
}

template<class I>
void BrokeStudioFirmware::writeFile(uint8 path, uint8 file, uint32 offset, I data_begin, I data_end) {
	std::vector<uint8>& f = this->files[path][file];
	auto const data_size = data_end - data_begin;
	uint32 const offset_end = offset + data_size;
	if (offset_end > f.size()) {
		f.resize(offset_end, 0);
	}

	for (std::vector<uint8>::size_type i = offset; i < offset_end; ++i) {
		f[i] = *data_begin;
		++data_begin;
	}
	this->file_exists[path][file] = true;
}

uint8 BrokeStudioFirmware::getFreeFileId(uint8 path) const {
	uint8 const NOT_FOUND = 128;
	if (path >= NUM_FILE_PATHS) {
		return NOT_FOUND;
	}
	std::array<bool, NUM_FILES> const& existing_files = this->file_exists.at(path);
	for (size_t i = 0; i < existing_files.size(); ++i) {
		if (!existing_files[i]) {
			return i;
		}
	}
	return NOT_FOUND;
}

void BrokeStudioFirmware::saveFiles() const {
	//TODO
}

void BrokeStudioFirmware::loadFiles() {
	//TODO
}

template<class I>
void BrokeStudioFirmware::sendMessageToServer(I begin, I end) {
#if RAINBOW_DEBUG >= 1
	FCEU_printf("RAINBOW message to send: ");
	for (I cur = begin; cur < end; ++cur) {
		FCEU_printf("%02x ", *cur);
	}
	FCEU_printf("\n");
#endif

	//TODO
}

template<class I>
void BrokeStudioFirmware::sendUdpDatagramToServer(I begin, I end) {
#if RAINBOW_DEBUG >= 1
	FCEU_printf("RAINBOW %lu udp datagram to send", wall_clock_milli());
#	if RAINBOW_DEBUG >= 2
	FCEU_printf(": ");
 	for (I cur = begin; cur < end; ++cur) {
 		FCEU_printf("%02x ", *cur);
 	}
#	endif
	FCEU_printf("\n");
#endif

	// Would be more elegant and copy data only once, but emscripten/val.h requires rtti enabled
	//emscripten::val::global("FCEM").call("udpSend", this->udp_socket, emscripten::val::array(begin, end));

	// Copy data twice (to ensure contiguous memory, then to create TypedArray), but it works
	// Note: we may use Module.HEAPU8 instead of getValue(), seems better at copying data. More info: https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#access-memory-from-javascript
	std::vector<uint8_t> copied(begin, end);
	EM_ASM({
		let copied_array = new Int8Array($1);
		for (let i = 0; i < $1; ++i) {
			copied_array[i] = getValue($0+i, "i8");
		}
		FCEM.udpSend($2, copied_array);
	}, copied.data(), copied.size(), this->udp_socket);
}

void BrokeStudioFirmware::receiveDataFromServer() {
	// Websocket
	//TODO

	// UDP
	if (this->udp_socket > -1) {
		static std::array<uint8_t, 254> incoming_buffer;
		int msg_len;
		do {
			// Fetch message from network implementation
			msg_len = EM_ASM_INT({
				// Get buffered message from server
				let raw_payload = FCEM.udpReceive($2);
				if (raw_payload === null) {
					return 0;
				}
				let payload = new Uint8Array(raw_payload);
				if (payload.length > $1) {
					console.error("ignoring message from server, too long: "+ payload.length +" bytes");
					return 0;
				}

				// Copy payload to C++ heap
				for (let i = 0; i < payload.length; ++i) {
					setValue($0+i, payload[i], "i8");
				}
				return payload.length;
			}, incoming_buffer.data(), incoming_buffer.size(), this->udp_socket);

			// Store fetched message
			if (msg_len > 0) {
				std::deque<uint8> message({
					static_cast<uint8>(msg_len+1),
					static_cast<uint8>(fromesp_cmds_t::MESSAGE_FROM_SERVER)
				});
				message.insert(message.end(), incoming_buffer.begin(), incoming_buffer.begin() + msg_len);
				this->tx_messages.push_back(message);
			}
		}while(msg_len > 0);
	}
}

void BrokeStudioFirmware::closeConnection() {
	//Close UDP socket
	EM_ASM({
		return FCEM.closeUdpSocket($0);
	}, this->udp_socket);
	this->udp_socket = -1;

	//TODO close TCP connection
	//TODO close ws socket
}

void BrokeStudioFirmware::openConnection() {
	this->closeConnection();

	if ((this->active_protocol == server_protocol_t::WEBSOCKET) || (this->active_protocol == server_protocol_t::WEBSOCKET_SECURED)) {
		//TODO
	}else if ((this->active_protocol == server_protocol_t::TCP) || (this->active_protocol == server_protocol_t::TCP_SECURED)) {
		//TODO
	}else if (this->active_protocol == server_protocol_t::UDP) {
		this->udp_socket = EM_ASM_INT({
			return FCEM.createUdpSocket(UTF8ToString($0), $1);
		}, this->server_settings_address.c_str(), this->server_settings_port);
		UDBG("RAINBOW got udp socket %d\n", this->udp_socket);
	}
}

void BrokeStudioFirmware::receivePingResult() {
	uint32_t min, max, avg, lost;
	int ready = EM_ASM_INT({
		let result = FCEM.popPingResult();
		if (result === null) {
			return 0;
		}

		let avg = 0;
		if (result.lost < result.number) {
			avg = result.total / (result.number - result.lost);
		}

		setValue($0, result.min, "i32");
		setValue($1, result.max, "i32");
		setValue($2, avg, "i32");
		setValue($3, result.lost, "i32");
		return 1;
	}, &min, &max, &avg, &lost);


	if (ready) {
		auto esp_time = [](uint32_t ms) -> uint8 {
			return (ms + 2) / 4;
		};
		UDBG(
			"Ping result: min:%dms(%d) max:%dms(%d) avg:%dms(%d) lost:%d\n",
			min, esp_time(min),
			max, esp_time(max),
			avg, esp_time(avg),
			lost
		);
		this->tx_messages.push_back({
			5,
			static_cast<uint8>(fromesp_cmds_t::SERVER_PING),
			esp_time(min),
			esp_time(max),
			esp_time(avg),
			static_cast<uint8>(lost)
		});
		this->ping_running = false;
	}
}

void BrokeStudioFirmware::downloadFile(std::string const& url, uint8 path, uint8 file) {
	//TODO implement that function
	this->tx_messages.push_back({
		2,
		static_cast<uint8>(fromesp_cmds_t::FILE_DOWNLOAD),
		static_cast<uint8>(file_download_results_t::NETWORK_ERROR),
		0,
		static_cast<uint8>(file_download_network_error_t::NOT_CONNECTED)
	});
}
