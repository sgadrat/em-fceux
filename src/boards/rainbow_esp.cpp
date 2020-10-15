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

	// Load file list from save file (if any)
	this->loadFiles();
}

BrokeStudioFirmware::~BrokeStudioFirmware() {
	UDBG("RAINBOW BrokeStudioFirmware dtor\n");
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
	UDBG_FLOOD("RAINBOW BrokeStudioFirmware tx\n");

	// Refresh buffer from network
	this->receiveDataFromServer();
	this->receivePingResult();

	// Fill buffer with the next message (if needed)
	if (this->tx_buffer.empty() && !this->tx_messages.empty()) {
		std::deque<uint8> message = this->tx_messages.front();
		this->tx_buffer.push_back(last_byte_read);
		this->tx_buffer.insert(this->tx_buffer.end(), message.begin(), message.end());
		this->tx_messages.pop_front();
	}

	// Get byte from buffer
	if (!this->tx_buffer.empty()) {
		last_byte_read = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	//UDBG("RAINBOW BrokeStudioFirmware tx <= %02x\n", last_byte_read);
	return last_byte_read;
}

void BrokeStudioFirmware::setGpio4(bool /*v*/) {
}

bool BrokeStudioFirmware::getGpio4() {
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
	switch (static_cast<n2e_cmds_t>(this->rx_buffer.at(1))) {
		case n2e_cmds_t::GET_ESP_STATUS:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_ESP_STATUS\n");
			this->tx_messages.push_back({1, static_cast<uint8>(e2n_cmds_t::READY)});
			break;
		case n2e_cmds_t::DEBUG_GET_LEVEL:
			UDBG("RAINBOW BrokeStudioFirmware received message DEBUG_GET_LEVEL\n");
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(e2n_cmds_t::DEBUG_LEVEL),
				static_cast<uint8>(this->debug_config)
				});
			break;
		case n2e_cmds_t::DEBUG_SET_LEVEL:
			UDBG("RAINBOW BrokeStudioFirmware received message DEBUG_SET_LEVEL\n");
			if (message_size == 3) {
				this->debug_config = this->rx_buffer.at(2);
			}
			FCEU_printf("DEBUG LEVEL SET TO: %u\n", this->debug_config);
			break;
		case n2e_cmds_t::DEBUG_LOG:
			UDBG("RAINBOW DEBUG/LOG\n");
			if (this->debug_config & 1) {
				for (std::deque<uint8>::const_iterator cur = this->rx_buffer.begin() + 2; cur < this->rx_buffer.end(); ++cur) {
					FCEU_printf("%02x ", *cur);
				}
				FCEU_printf("\n");
			}
			break;
		case n2e_cmds_t::CLEAR_BUFFERS:
			UDBG("RAINBOW BrokeStudioFirmware received message CLEAR_BUFFERS\n");
			this->receiveDataFromServer();
			this->receivePingResult();
			this->tx_buffer.clear();
			this->tx_messages.clear();
			this->rx_buffer.clear();
			break;
		case n2e_cmds_t::E2N_BUFFER_DROP:
			UDBG("RAINBOW BrokeStudioFirmware received message E2N_BUFFER_DROP\n");
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
		case n2e_cmds_t::GET_WIFI_STATUS:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_WIFI_STATUS\n");
			this->tx_messages.push_back({2, static_cast<uint8>(e2n_cmds_t::WIFI_STATUS), 3}); // Simple answer, wifi is ok
			break;
		case n2e_cmds_t::GET_RND_BYTE:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_BYTE\n");
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(e2n_cmds_t::RND_BYTE),
				static_cast<uint8>(rand() % 256)
			});
			break;
		case n2e_cmds_t::GET_RND_BYTE_RANGE: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_BYTE_RANGE\n");
			int const min_value = this->rx_buffer.at(2);
			int const max_value = this->rx_buffer.at(3);
			int const range = max_value - min_value;
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(e2n_cmds_t::RND_BYTE),
				static_cast<uint8>(min_value + (rand() % range))
			});
			break;
		}
		case n2e_cmds_t::GET_RND_WORD:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_WORD\n");
			this->tx_messages.push_back({
				3,
				static_cast<uint8>(e2n_cmds_t::RND_WORD),
				static_cast<uint8>(rand() % 256),
				static_cast<uint8>(rand() % 256)
			});
			break;
		case n2e_cmds_t::GET_RND_WORD_RANGE: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_WORD_RANGE\n");
			int const min_value = (static_cast<int>(this->rx_buffer.at(2)) << 8) + this->rx_buffer.at(3);
			int const max_value = (static_cast<int>(this->rx_buffer.at(4)) << 8) + this->rx_buffer.at(5);
			int const range = max_value - min_value;
			int const rand_value = min_value + (rand() % range);
			this->tx_messages.push_back({
				3,
				static_cast<uint8>(e2n_cmds_t::RND_WORD),
				static_cast<uint8>(rand_value >> 8),
				static_cast<uint8>(rand_value & 0xff)
			});
			break;
		}
		case n2e_cmds_t::GET_SERVER_STATUS: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_STATUS\n");
			uint8 status;
			switch (this->active_protocol) {
			case server_protocol_t::WEBSOCKET:
				status = 0; //TODO
				break;
			case server_protocol_t::UDP:
				status = 0; //TODO
				break;
			default:
				status = 0; // Unknown active protocol, connection certainly broken
			}

			this->tx_messages.push_back({
				2,
				static_cast<uint8>(e2n_cmds_t::SERVER_STATUS),
				status
			});
			break;
		}
		case n2e_cmds_t::GET_SERVER_PING:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_PING\n");
			if (!this->ping_running) {
				if (this->server_settings_address.empty()) {
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(e2n_cmds_t::SERVER_PING)
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
		case n2e_cmds_t::SET_SERVER_PROTOCOL: {
			UDBG("RAINBOW BrokeStudioFirmware received message SET_SERVER_PROTOCOL\n");
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
		case n2e_cmds_t::GET_SERVER_SETTINGS: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_SETTINGS\n");
			if (this->server_settings_address.empty() && this->server_settings_port == 0) {
				this->tx_messages.push_back({
					1,
					static_cast<uint8>(e2n_cmds_t::SERVER_SETTINGS)
				});
			}else {
				std::deque<uint8> message({
					static_cast<uint8>(1 + 2 + this->server_settings_address.size()),
					static_cast<uint8>(e2n_cmds_t::SERVER_SETTINGS),
					static_cast<uint8>(this->server_settings_port >> 8),
					static_cast<uint8>(this->server_settings_port & 0xff)
				});
				message.insert(message.end(), this->server_settings_address.begin(), this->server_settings_address.end());
				this->tx_messages.push_back(message);
			}
			break;
		}
		case n2e_cmds_t::GET_SERVER_CONFIG_SETTINGS: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_CONFIG_SETTINGS\n");
			if (this->default_server_settings_address.empty() && this->default_server_settings_port == 0) {
				this->tx_messages.push_back({
					1,
					static_cast<uint8>(e2n_cmds_t::SERVER_SETTINGS)
					});
			}
			else {
				std::deque<uint8> message({
					static_cast<uint8>(1 + 2 + this->default_server_settings_address.size()),
					static_cast<uint8>(e2n_cmds_t::SERVER_SETTINGS),
					static_cast<uint8>(this->default_server_settings_port >> 8),
					static_cast<uint8>(this->default_server_settings_port & 0xff)
					});
				message.insert(message.end(), this->default_server_settings_address.begin(), this->default_server_settings_address.end());
				this->tx_messages.push_back(message);
			}
			break;
		}
		case n2e_cmds_t::SET_SERVER_SETTINGS:
			UDBG("RAINBOW BrokeStudioFirmware received message SET_SERVER_SETTINGS\n");
			if (message_size >= 3) {
				this->server_settings_port =
					(static_cast<uint16_t>(this->rx_buffer.at(2)) << 8) +
					(static_cast<uint16_t>(this->rx_buffer.at(3)))
				;
				this->server_settings_address = std::string(this->rx_buffer.begin()+4, this->rx_buffer.end());
			}
			break;
		case n2e_cmds_t::RESTORE_SERVER_SETTINGS:
			UDBG("RAINBOW BrokeStudioFirmware received message RESTORE_SERVER_SETTINGS\n");
			this->server_settings_address = this->default_server_settings_address;
			this->server_settings_port = this->default_server_settings_port;
			break;
		case n2e_cmds_t::CONNECT_TO_SERVER:
			UDBG("RAINBOW BrokeStudioFirmware received message CONNECT_TO_SERVER\n");
			this->openConnection();
			break;
		case n2e_cmds_t::DISCONNECT_FROM_SERVER:
			UDBG("RAINBOW BrokeStudioFirmware received message DISCONNECT_FROM_SERVER\n");
			this->closeConnection();
			break;
		case n2e_cmds_t::SEND_MESSAGE_TO_SERVER: {
			UDBG("RAINBOW BrokeStudioFirmware received message SEND_MESSAGE\n");
			uint8 const payload_size = this->rx_buffer.size() - 2;
			std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
			std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;

			switch (this->active_protocol) {
			case server_protocol_t::WEBSOCKET:
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
		case n2e_cmds_t::FILE_OPEN:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_OPEN\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES) {
					this->file_exists[path][file] = true;
					this->working_path = path;
					this->working_file = file;
					this->file_offset = 0;
					this->saveFiles();
				}
			}
			break;
		case n2e_cmds_t::FILE_CLOSE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_CLOSE\n");
			this->working_file = NO_WORKING_FILE;
			this->saveFiles();
			break;
		case n2e_cmds_t::FILE_EXISTS:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_EXISTS\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES) {
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_EXISTS),
						static_cast<uint8>(this->file_exists[path][file] ? 1 : 0)
					});
				}
			}
			break;
		case n2e_cmds_t::FILE_DELETE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_DELETE\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES) {
					if (this->file_exists[path][file]) {
						// File exists, let's delete it
						this->files[path][file].clear();
						this->file_exists[path][file] = false;
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(e2n_cmds_t::FILE_DELETE),
							0
						});
						this->saveFiles();
					}else {
						// File does not exist
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(e2n_cmds_t::FILE_DELETE),
							2
						});
					}
				}else {
					// Error while deleting the file
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_DELETE),
						1
					});
				}
			}
			break;
		case n2e_cmds_t::FILE_SET_CUR:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_SET_CUR\n");
			if (2 <= message_size && message_size <= 5) {
				this->file_offset = this->rx_buffer[2];
				this->file_offset += static_cast<uint32>(message_size >= 3 ? this->rx_buffer[3] : 0) << 8;
				this->file_offset += static_cast<uint32>(message_size >= 4 ? this->rx_buffer[4] : 0) << 16;
				this->file_offset += static_cast<uint32>(message_size >= 5 ? this->rx_buffer[5] : 0) << 24;
			}
			break;
		case n2e_cmds_t::FILE_READ:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_READ\n");
			if (message_size == 2) {
				if (this->working_file != NO_WORKING_FILE) {
					uint8 const n = this->rx_buffer[2];
					this->readFile(this->working_path, this->working_file, n, this->file_offset);
					this->file_offset += n;
					UDBG("working file offset: %u (%x)\n", this->file_offset, this->file_offset);
					UDBG("file size: %lu bytes\n", this->files[this->working_path][this->working_file].size());
					if (this->file_offset > this->files[this->working_path][this->working_file].size()) {
						this->file_offset = this->files[this->working_path][this->working_file].size();
					}
				}else {
					this->tx_messages.push_back({2, static_cast<uint8>(e2n_cmds_t::FILE_DATA), 0});
				}
			}
			break;
		case n2e_cmds_t::FILE_WRITE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_WRITE\n");
			if (message_size >= 3 && this->working_file != NO_WORKING_FILE) {
				this->writeFile(this->working_path, this->working_file, this->file_offset, this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
				this->file_offset += message_size - 1;
			}
			break;
		case n2e_cmds_t::FILE_APPEND:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_APPEND\n");
			if (message_size >= 3 && this->working_file != NO_WORKING_FILE) {
				this->writeFile(this->working_path, this->working_file, this->files[working_path][working_file].size(), this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
			}
			break;
		case n2e_cmds_t::FILE_COUNT:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_COUNT\n");
			if (message_size == 2) {
				uint8 const path = this->rx_buffer[2];
				if (path >= NUM_FILE_PATHS) {
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_COUNT),
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
						static_cast<uint8>(e2n_cmds_t::FILE_COUNT),
						nb_files
					});
					UDBG("%u files found in path %u\n", nb_files, path);
				}
			}
			break;
		case n2e_cmds_t::FILE_GET_LIST:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_LIST\n");
			if (message_size >= 2) {
				std::vector<uint8> existing_files;
				uint8 const path = this->rx_buffer[2];
				uint8 page_size = NUM_FILES;
				uint8 current_page = 0;
				if (message_size == 4) {
					page_size = this->rx_buffer[3];
					current_page = this->rx_buffer[4];
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
					static_cast<uint8>(e2n_cmds_t::FILE_LIST),
					static_cast<uint8>(existing_files.size())
				});
				message.insert(message.end(), existing_files.begin(), existing_files.end());
				this->tx_messages.push_back(message);
			}
			break;
		case n2e_cmds_t::FILE_GET_FREE_ID:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_FREE_ID\n");
			if (message_size == 2) {
				uint8 const file_id = this->getFreeFileId(this->rx_buffer.at(2));
				if (file_id != 128) {
					// Free file ID found
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_ID),
						file_id,
					});
				}else {
					// Free file ID not found
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(e2n_cmds_t::FILE_ID)
					});
				}
			}
			break;
		case n2e_cmds_t::FILE_GET_INFO:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_INFO\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES && this->file_exists[path][file]) {
					// Compute info
					uint32 file_crc32;
					file_crc32 = CalcCRC32(0L, this->files[path][file].data(), this->files[path][file].size());

					uint32 file_size = this->files[path][file].size();

					// Send info
					this->tx_messages.push_back({
						9,
						static_cast<uint8>(e2n_cmds_t::FILE_INFO),

						static_cast<uint8>((file_crc32 >> 24) & 0xff),
						static_cast<uint8>((file_crc32 >> 16) & 0xff),
						static_cast<uint8>((file_crc32 >> 8) & 0xff),
						static_cast<uint8>(file_crc32 & 0xff),

						static_cast<uint8>((file_size >> 24) & 0xff),
						static_cast<uint8>((file_size >> 16) & 0xff),
						static_cast<uint8>((file_size >> 8 ) & 0xff),
						static_cast<uint8>(file_size & 0xff)
					});
				}else {
					// File not found or path/file out of bounds
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(e2n_cmds_t::FILE_INFO)
					});
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
		static_cast<uint8>(e2n_cmds_t::FILE_DATA),
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
	FCEU_printf("RAINBOW udp datagram to send: ");
	for (I cur = begin; cur < end; ++cur) {
		FCEU_printf("%02x ", *cur);
	}
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
					static_cast<uint8>(e2n_cmds_t::MESSAGE_FROM_SERVER)
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

	//TODO close ws socket
}

void BrokeStudioFirmware::openConnection() {
	this->closeConnection();

	if (this->active_protocol == server_protocol_t::WEBSOCKET) {
		//TODO
	}else {
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
			static_cast<uint8>(e2n_cmds_t::SERVER_PING),
			esp_time(min),
			esp_time(max),
			esp_time(avg),
			static_cast<uint8>(lost)
		});
		this->ping_running = false;
	}
}
