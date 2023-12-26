#include "rainbow_esp.h"

#include "../fceu.h"
#include "../utils/crc32.h"

#include <emscripten.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

#ifndef RAINBOW_DEBUG_ESP
#define RAINBOW_DEBUG_ESP 0
#endif

#if RAINBOW_DEBUG_ESP >= 1
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

#if RAINBOW_DEBUG_ESP >= 2
#define UDBG_FLOOD(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG_FLOOD(...)
#endif

#if RAINBOW_DEBUG_ESP >= 1
#include "../debug.h"
namespace
{
	uint64_t wall_clock_milli()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	};
}
#endif

namespace
{
	std::array<std::string, NUM_FILE_PATHS> dir_names = { "save", "roms", "user" };
}

BrokeStudioFirmware::BrokeStudioFirmware()
{
	UDBG("[Rainbow] BrokeStudioFirmware constructor\n");

	// Get default host/port
	char const *hostname = ::getenv("RAINBOW_SERVER_ADDR");
	if (hostname == nullptr)
		hostname = "";
	this->server_settings_address = hostname;
	this->default_server_settings_address = hostname;

	char const *port_cstr = ::getenv("RAINBOW_SERVER_PORT");
	if (port_cstr == nullptr)
		port_cstr = "0";
	std::istringstream port_iss(port_cstr);
	port_iss >> this->server_settings_port;
	this->default_server_settings_port = this->server_settings_port;

	// Init fake registered networks
	this->networks = {{
		{"EMULATOR_SSID", "EMULATOR_PASS", true},
		{"", "", false},
		{"", "", false},
	}};

	// Load file list from save file (if any)
	this->loadFiles();

	// Initialize download system
	this->initDownload();
}

BrokeStudioFirmware::~BrokeStudioFirmware()
{
	UDBG("[Rainbow] BrokeStudioFirmware destructor\n");
	this->closeConnection();
	this->cleanupDownload();
}

void BrokeStudioFirmware::rx(uint8_t v)
{
	UDBG_FLOOD("RAINBOW BrokeStudioFirmware rx %02x\n", v);
	if (this->msg_first_byte)
	{
		this->msg_first_byte = false;
		this->msg_length = v + 1;
	}
	this->rx_buffer.push_back(v);

	if (this->rx_buffer.size() == this->msg_length)
	{
		this->processBufferedMessage();
		this->msg_first_byte = true;
	}
}

uint8_t BrokeStudioFirmware::tx()
{
	// Refresh buffer from network
	this->receiveDataFromServer();
	this->receivePingResult();

	// Fill buffer with the next message (if needed)
	if (this->tx_buffer.empty() && !this->tx_messages.empty())
	{
		std::deque<uint8_t> message = this->tx_messages.front();
		this->tx_buffer.insert(this->tx_buffer.end(), message.begin(), message.end());
		this->tx_messages.pop_front();
	}

	// Get byte from buffer
	if (!this->tx_buffer.empty())
	{
		last_byte_read = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	UDBG_FLOOD("RAINBOW BrokeStudioFirmware tx %02x\n", last_byte_read);
	return last_byte_read;
}

bool BrokeStudioFirmware::getDataReadyIO()
{
	this->receiveDataFromServer();
	this->receivePingResult();
	return !(this->tx_buffer.empty() && this->tx_messages.empty());
}

void BrokeStudioFirmware::processBufferedMessage()
{
	assert(this->rx_buffer.size() >= 2); // Buffer must contain exactly one message, minimal message is two bytes (length + type)
	uint8_t const message_size = this->rx_buffer.front();
	assert(message_size >= 1); // minimal payload is one byte (type)
	assert(this->rx_buffer.size() == static_cast<std::deque<uint8_t>::size_type>(message_size) + 1); // Buffer size must match declared payload size

	// Process the message in RX buffer
	switch (static_cast<toesp_cmds_t>(this->rx_buffer.at(1)))
	{

		// ESP CMDS

	case toesp_cmds_t::ESP_GET_STATUS:
		UDBG("[Rainbow] ESP received command ESP_GET_STATUS\n");
		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::READY),
			static_cast<uint8_t>(isSdCardFilePresent ? 1 : 0)
		});
		break;
	case toesp_cmds_t::DEBUG_GET_LEVEL:
		UDBG("[Rainbow] ESP received command DEBUG_GET_LEVEL\n");
		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::DEBUG_LEVEL),
			static_cast<uint8_t>(this->debug_config)
		});
		break;
	case toesp_cmds_t::DEBUG_SET_LEVEL:
		UDBG("[Rainbow] ESP received command DEBUG_SET_LEVEL\n");
		if (message_size == 2)
		{
			this->debug_config = this->rx_buffer.at(2);
		}
		FCEU_printf("DEBUG LEVEL SET TO: %u\n", this->debug_config);
		break;
	case toesp_cmds_t::DEBUG_LOG:
	{
		UDBG("[Rainbow] ESP received command DEBUG_LOG\n");
		static bool isRainbowDebugEnabled = RAINBOW_DEBUG_ESP > 0;
		if (isRainbowDebugEnabled || (this->debug_config & 1))
		{
			for (std::deque<uint8_t>::const_iterator cur = this->rx_buffer.begin() + 2; cur < this->rx_buffer.end(); ++cur)
			{
				FCEU_printf("%02x ", *cur);
			}
			FCEU_printf("\n");
		}
		break;
	}
	case toesp_cmds_t::BUFFER_CLEAR_RX_TX:
		UDBG("[Rainbow] ESP received command BUFFER_CLEAR_RX_TX\n");
		this->receiveDataFromServer();
		this->receivePingResult();
		this->tx_buffer.clear();
		this->tx_messages.clear();
		this->rx_buffer.clear();
		break;
	case toesp_cmds_t::BUFFER_DROP_FROM_ESP:
		UDBG("[Rainbow] ESP received command BUFFER_DROP_FROM_ESP\n");
		if (message_size == 3)
		{
			uint8_t const message_type = this->rx_buffer.at(2);
			uint8_t const n_keep = this->rx_buffer.at(3);

			size_t i = 0;
			for (std::deque<std::deque<uint8_t>>::iterator message = this->tx_messages.end();
				message != this->tx_messages.begin();)
			{
				--message;
				if (message->at(1) == message_type)
				{
					++i;
					if (i > n_keep)
					{
						UDBG("[Rainbow] ESP erase message: index=%ld\n", message - this->tx_messages.begin());
						message = this->tx_messages.erase(message);
					}
					else
					{
						UDBG("[Rainbow] ESP keep message: index=%ld - too recent\n", message - this->tx_messages.begin());
					}
				}
				else
				{
					UDBG("[Rainbow] ESP keep message: index=%ld - bad type\n", message - this->tx_messages.begin());
				}
			}
		}
		break;
	case toesp_cmds_t::ESP_GET_FIRMWARE_VERSION:
		UDBG("[Rainbow] ESP received command ESP_GET_FIRMWARE_VERSION\n");
		this->tx_messages.push_back({19, static_cast<uint8_t>(fromesp_cmds_t::ESP_FIRMWARE_VERSION), 17, 'E', 'M', 'U', 'L', 'A', 'T', 'O', 'R', '_', 'F', 'I', 'R', 'M', 'W', 'A', 'R', 'E'});
		break;

	case toesp_cmds_t::ESP_FACTORY_SETTINGS:
		UDBG("[Rainbow] ESP received command ESP_FACTORY_SETTINGS\n");
		UDBG("[Rainbow] ESP_FACTORY_SETTINGS has no use here\n");
		this->tx_messages.push_back({2, static_cast<uint8_t>(fromesp_cmds_t::ESP_FACTORY_RESET), static_cast<uint8_t>(esp_factory_reset::ERROR_WHILE_RESETTING_CONFIG)});
		break;

	case toesp_cmds_t::ESP_RESTART:
		UDBG("[Rainbow] ESP received command ESP_RESTART\n");
		UDBG("[Rainbow] ESP_RESTART has no use here\n");
		break;

		// WIFI CMDS

	case toesp_cmds_t::WIFI_GET_STATUS:
		UDBG("[Rainbow] ESP received command WIFI_GET_STATUS\n");
		this->tx_messages.push_back({3, static_cast<uint8_t>(fromesp_cmds_t::WIFI_STATUS), 3, 0}); // Simple answer, wifi is ok
		break;
		// WIFI_GET_SSID/WIFI_GET_IP config commands are not relevant here, so we'll just use fake data
	case toesp_cmds_t::WIFI_GET_SSID:
		UDBG("[Rainbow] ESP received command WIFI_GET_SSID\n");
		if ((this->wifi_config & static_cast<uint8_t>(wifi_config_t::WIFI_ENABLE)) == static_cast<uint8_t>(wifi_config_t::WIFI_ENABLE))
		{
			this->tx_messages.push_back({15, static_cast<uint8_t>(fromesp_cmds_t::SSID), 13, 'E', 'M', 'U', 'L', 'A', 'T', 'O', 'R', '_', 'S', 'S', 'I', 'D'});
		}
		else
		{
			this->tx_messages.push_back({2, static_cast<uint8_t>(fromesp_cmds_t::SSID), 0});
		}
		break;
	case toesp_cmds_t::WIFI_GET_IP:
		UDBG("[Rainbow] ESP received command WIFI_GET_IP\n");
		if ((this->wifi_config & static_cast<uint8_t>(wifi_config_t::WIFI_ENABLE)) == static_cast<uint8_t>(wifi_config_t::WIFI_ENABLE))
		{
			this->tx_messages.push_back({14, static_cast<uint8_t>(fromesp_cmds_t::IP_ADDRESS), 12, '1', '9', '2', '.', '1', '6', '8', '.', '1', '.', '1', '0'});
		}
		else
		{
			this->tx_messages.push_back({2, static_cast<uint8_t>(fromesp_cmds_t::IP_ADDRESS), 0});
		}
		break;
	case toesp_cmds_t::WIFI_GET_CONFIG:
		UDBG("[Rainbow] ESP received command WIFI_GET_CONFIG\n");
		this->tx_messages.push_back({2, static_cast<uint8_t>(fromesp_cmds_t::WIFI_CONFIG), this->wifi_config});
		break;
	case toesp_cmds_t::WIFI_SET_CONFIG:
		UDBG("[Rainbow] ESP received command WIFI_SET_CONFIG\n");
		this->wifi_config = this->rx_buffer.at(2);
		break;

		// AP CMDS
		// AP_GET_SSID/AP_GET_IP config commands are not relevant here, so we'll just use fake data
	case toesp_cmds_t::AP_GET_SSID:
		UDBG("[Rainbow] ESP received command AP_GET_SSID\n");
		if ((this->wifi_config & static_cast<uint8_t>(wifi_config_t::AP_ENABLE)) == static_cast<uint8_t>(wifi_config_t::AP_ENABLE))
		{
			this->tx_messages.push_back({18, static_cast<uint8_t>(fromesp_cmds_t::SSID), 16, 'E', 'M', 'U', 'L', 'A', 'T', 'O', 'R', '_', 'A', 'P', '_', 'S', 'S', 'I', 'D'});
		}
		else
		{
			this->tx_messages.push_back({2, static_cast<uint8_t>(fromesp_cmds_t::SSID), 0});
		}
		break;
	case toesp_cmds_t::AP_GET_IP:
		UDBG("[Rainbow] ESP received command AP_GET_ID\n");
		if ((this->wifi_config & static_cast<uint8_t>(wifi_config_t::AP_ENABLE)) == static_cast<uint8_t>(wifi_config_t::AP_ENABLE))
		{
			this->tx_messages.push_back({16, static_cast<uint8_t>(fromesp_cmds_t::IP_ADDRESS), 14, '1', '2', '7', '.', '0', '.', '0', '.', '1', ':', '8', '0', '8', '0'});
		}
		else
		{
			this->tx_messages.push_back({2, static_cast<uint8_t>(fromesp_cmds_t::IP_ADDRESS), 0});
		}
		break;

		// RND CMDS

	case toesp_cmds_t::RND_GET_BYTE:
		UDBG("[Rainbow] ESP received command RND_GET_BYTE\n");
		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::RND_BYTE),
			static_cast<uint8_t>(rand() % 256)
		});
		break;
	case toesp_cmds_t::RND_GET_BYTE_RANGE:
	{
		UDBG("[Rainbow] ESP received command RND_GET_BYTE_RANGE\n");
		if (message_size < 3)
		{
			break;
		}
		int const min_value = this->rx_buffer.at(2);
		int const max_value = this->rx_buffer.at(3);
		int const range = max_value - min_value;
		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::RND_BYTE),
			static_cast<uint8_t>(min_value + (rand() % range))
		});
		break;
	}
	case toesp_cmds_t::RND_GET_WORD:
		UDBG("[Rainbow] ESP received command RND_GET_WORD\n");
		this->tx_messages.push_back({
			3,
			static_cast<uint8_t>(fromesp_cmds_t::RND_WORD),
			static_cast<uint8_t>(rand() % 256),
			static_cast<uint8_t>(rand() % 256)
		});
		break;
	case toesp_cmds_t::RND_GET_WORD_RANGE:
	{
		UDBG("[Rainbow] ESP received command RND_GET_WORD_RANGE\n");
		if (message_size < 5)
		{
			break;
		}
		int const min_value = (static_cast<int>(this->rx_buffer.at(2)) << 8) + this->rx_buffer.at(3);
		int const max_value = (static_cast<int>(this->rx_buffer.at(4)) << 8) + this->rx_buffer.at(5);
		int const range = max_value - min_value;
		int const rand_value = min_value + (rand() % range);
		this->tx_messages.push_back({
			3,
			static_cast<uint8_t>(fromesp_cmds_t::RND_WORD),
			static_cast<uint8_t>(rand_value >> 8),
			static_cast<uint8_t>(rand_value & 0xff)
		});
		break;
	}

		// SERVER CMDS

	case toesp_cmds_t::SERVER_GET_STATUS:
	{
		UDBG("[Rainbow] ESP received command SERVER_GET_STATUS\n");
		uint8_t status;
		switch (this->active_protocol)
		{
		case server_protocol_t::TCP:
			// TODO actually check connection state
			status = (this->tcp_socket != -1); // Considere server connection ok if we created a socket
			break;
		case server_protocol_t::TCP_SECURED:
			// TODO
			status = 0;
			break;
		case server_protocol_t::UDP:
			status = (this->udp_socket != -1); // Considere server connection ok if we created a socket
			break;
		default:
			status = 0; // Unknown active protocol, connection certainly broken
		}

		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::SERVER_STATUS),
			status
		});
		break;
	}
	case toesp_cmds_t::SERVER_PING:
		UDBG("[Rainbow] ESP received command SERVER_PING\n");
		if (!this->ping_running)
		{
			if (this->server_settings_address.empty())
			{
				this->tx_messages.push_back({
					1,
					static_cast<uint8_t>(fromesp_cmds_t::SERVER_PING)
				});
			}
			else if (message_size >= 1)
			{
				this->ping_running = true;
				uint8_t n = (message_size == 1 ? 0 : this->rx_buffer.at(2));
				if (n == 0)
				{
					n = 4;
				}
				EM_ASM({
					FCEM.pingServer(UTF8ToString($0), $1, $2);
				}, this->server_settings_address.c_str(), this->server_settings_port, n);
			}
		}
		break;
	case toesp_cmds_t::SERVER_SET_PROTOCOL:
	{
		UDBG("[Rainbow] ESP received command SERVER_SET_PROTOCOL\n");
		if (message_size == 2)
		{
			server_protocol_t requested_protocol = static_cast<server_protocol_t>(this->rx_buffer.at(2));
			if (requested_protocol > server_protocol_t::UDP)
			{
				UDBG("[Rainbow] ESP SET_SERVER_PROTOCOL: unknown protocol (%u)\n", static_cast<unsigned int>(requested_protocol));
			}
			else
			{
				if (requested_protocol == server_protocol_t::TCP_SECURED)
				{
					UDBG("[Rainbow] SET_SERVER_PROTOCOL: protocol TCP_SECURED not supported, falling back to TCP\n");
					requested_protocol = server_protocol_t::TCP;
				}
				this->active_protocol = requested_protocol;
			}
		}
		break;
	}
	case toesp_cmds_t::SERVER_GET_SETTINGS:
	{
		UDBG("[Rainbow] ESP received command SERVER_GET_SETTINGS\n");
		if (this->server_settings_address.empty() && this->server_settings_port == 0)
		{
			this->tx_messages.push_back({
				1,
				static_cast<uint8_t>(fromesp_cmds_t::SERVER_SETTINGS)
			});
		}
		else
		{
			std::deque<uint8_t> message({
				static_cast<uint8_t>(1 + 2 + 1 + this->server_settings_address.size()),
				static_cast<uint8_t>(fromesp_cmds_t::SERVER_SETTINGS),
				static_cast<uint8_t>(this->server_settings_port >> 8),
				static_cast<uint8_t>(this->server_settings_port & 0xff),
				static_cast<uint8_t>(this->server_settings_address.size())
			});
			message.insert(message.end(), this->server_settings_address.begin(), this->server_settings_address.end());
			this->tx_messages.push_back(message);
		}
		break;
	}
	case toesp_cmds_t::SERVER_SET_SETTINGS:
		UDBG("[Rainbow] ESP received command SERVER_SET_SETTINGS\n");
		if (message_size >= 5)
		{
			this->server_settings_port =
				(static_cast<uint16_t>(this->rx_buffer.at(2)) << 8) +
				(static_cast<uint16_t>(this->rx_buffer.at(3)));
			uint8_t len = this->rx_buffer.at(4);
			this->server_settings_address = std::string(this->rx_buffer.begin() + 5, this->rx_buffer.begin() + 5 + len);
		}
		break;
	case toesp_cmds_t::SERVER_GET_SAVED_SETTINGS:
	{
		UDBG("[Rainbow] ESP received command SERVER_GET_SAVED_SETTINGS\n");
		if (this->default_server_settings_address.empty() && this->default_server_settings_port == 0)
		{
			this->tx_messages.push_back({
				1,
				static_cast<uint8_t>(fromesp_cmds_t::SERVER_SETTINGS)
			});
		}
		else
		{
			std::deque<uint8_t> message({
				static_cast<uint8_t>(1 + 2 + 1 + this->default_server_settings_address.size()),
				static_cast<uint8_t>(fromesp_cmds_t::SERVER_SETTINGS),
				static_cast<uint8_t>(this->default_server_settings_port >> 8),
				static_cast<uint8_t>(this->default_server_settings_port & 0xff),
				static_cast<uint8_t>(this->server_settings_address.size())
			});
			message.insert(message.end(), this->default_server_settings_address.begin(), this->default_server_settings_address.end());
			this->tx_messages.push_back(message);
		}
		break;
	}
	case toesp_cmds_t::SERVER_SET_SAVED_SETTINGS:
	{
		UDBG("[Rainbow] ESP received command SERVER_SET_SAVED_SETTINGS\n");
		if (message_size == 1)
		{
			this->default_server_settings_port = 0;
			this->default_server_settings_address = "";
		} else if (message_size >= 5)
		{
			this->default_server_settings_port =
				(static_cast<uint16_t>(this->rx_buffer.at(2)) << 8) +
				(static_cast<uint16_t>(this->rx_buffer.at(3)));
			uint8_t len = this->rx_buffer.at(4);
			this->default_server_settings_address = std::string(this->rx_buffer.begin() + 5, this->rx_buffer.begin() + 5 + len);
			this->server_settings_port = this->default_server_settings_port;
			this->server_settings_address = default_server_settings_address;
		}
		break;
	}
	case toesp_cmds_t::SERVER_RESTORE_SAVED_SETTINGS:
		UDBG("[Rainbow] ESP received command SERVER_RESTORE_SAVED_SETTINGS\n");
		this->server_settings_address = this->default_server_settings_address;
		this->server_settings_port = this->default_server_settings_port;
		break;
	case toesp_cmds_t::SERVER_CONNECT:
		UDBG("[Rainbow] ESP received command SERVER_CONNECT\n");
		this->openConnection();
		break;
	case toesp_cmds_t::SERVER_DISCONNECT:
		UDBG("[Rainbow] ESP received command SERVER_DISCONNECT\n");
		this->closeConnection();
		break;
	case toesp_cmds_t::SERVER_SEND_MSG:
	{
		UDBG("[Rainbow] ESP received command SERVER_SEND_MSG\n");
		uint8_t const payload_size = static_cast<const uint8_t>(this->rx_buffer.size() - 2);
		std::deque<uint8_t>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
		std::deque<uint8_t>::const_iterator payload_end = payload_begin + payload_size;

		switch (this->active_protocol)
		{
		case server_protocol_t::TCP:
			this->sendTcpDataToServer(payload_begin, payload_end);
			break;
		case server_protocol_t::TCP_SECURED:
			UDBG("[Rainbow] ESP protocol TCP_SECURED not implemented\n");
			break;
		case server_protocol_t::UDP:
			this->sendUdpDatagramToServer(payload_begin, payload_end);
			break;
		default:
			UDBG("[Rainbow] ESP protocol (%u) not implemented\n", static_cast<unsigned int>(this->active_protocol));
		};
		break;
	}

	// NETWORK CMDS
	// network commands are not relevant here, so we'll just use test/fake data
	case toesp_cmds_t::NETWORK_SCAN:
		UDBG("[Rainbow] ESP received command NETWORK_SCAN\n");
		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::NETWORK_SCAN_RESULT),
			NUM_FAKE_NETWORKS
		});
		break;
	case toesp_cmds_t::NETWORK_GET_SCAN_RESULT:
		UDBG("[Rainbow] ESP received command NETWORK_GET_SCAN_RESULT\n");
		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::NETWORK_SCAN_RESULT),
			NUM_FAKE_NETWORKS
		});
		break;
	case toesp_cmds_t::NETWORK_GET_DETAILS:
		UDBG("[Rainbow] ESP received command NETWORK_GET_DETAILS\n");
		if (message_size == 2)
		{
			uint8_t networkItem = this->rx_buffer.at(2);
			if (networkItem > NUM_FAKE_NETWORKS - 1)
				networkItem = NUM_FAKE_NETWORKS - 1;
			this->tx_messages.push_back({
				24,
				static_cast<uint8_t>(fromesp_cmds_t::NETWORK_SCANNED_DETAILS),
				4, // encryption type
				0x47, // RSSI
				0x00,0x00,0x00,0x01, // channel
				0, // hidden?
				15, // SSID length
				'E','M','U','L','A','T','O','R','_','S','S','I','D','_',static_cast<uint8_t>(networkItem + '0') // SSID
			});
		}
		break;
	case toesp_cmds_t::NETWORK_GET_REGISTERED:
		UDBG("[Rainbow] ESP received command NETWORK_GET_REGISTERED\n");
		if (message_size == 1)
		{
			this->tx_messages.push_back({
				NUM_NETWORKS + 1,
				static_cast<uint8_t>(fromesp_cmds_t::NETWORK_REGISTERED),
				static_cast<uint8_t>((this->networks[0].ssid != "") ? 1 : 0),
				static_cast<uint8_t>((this->networks[1].ssid != "") ? 1 : 0),
				static_cast<uint8_t>((this->networks[2].ssid != "") ? 1 : 0)
			});
		}
		break;
	case toesp_cmds_t::NETWORK_GET_REGISTERED_DETAILS:
		UDBG("[Rainbow] ESP received command NETWORK_GET_REGISTERED_DETAILS\n");
		if (message_size == 2)
		{
			uint8_t networkItem = this->rx_buffer.at(2);
			if (networkItem > NUM_NETWORKS - 1)
				networkItem = NUM_NETWORKS - 1;
			std::deque<uint8_t> message({
				static_cast<uint8_t>(2 + 1 + this->networks[networkItem].ssid.length() + 1 + this->networks[networkItem].pass.length()),
				static_cast<uint8_t>(fromesp_cmds_t::NETWORK_REGISTERED_DETAILS),
				static_cast<uint8_t>(this->networks[networkItem].active ? 1 : 0),
				static_cast<uint8_t>(this->networks[networkItem].ssid.length())
			});
			message.insert(message.end(), this->networks[networkItem].ssid.begin(), this->networks[networkItem].ssid.end());
			message.insert(message.end(), static_cast<const uint8_t>(this->networks[networkItem].pass.length()));
			message.insert(message.end(), this->networks[networkItem].pass.begin(), this->networks[networkItem].pass.end());
			this->tx_messages.push_back(message);
		}
		break;
	case toesp_cmds_t::NETWORK_REGISTER:
		UDBG("[Rainbow] ESP received command NETWORK_REGISTER\n");
		if (message_size >= 8)
		{
			uint8_t const networkItem = this->rx_buffer.at(2);
			if (networkItem > NUM_NETWORKS - 1)
				break;
			bool const networkActive = this->rx_buffer.at(3) == 0 ? false : true;
			if (networkActive)
			{
				for (size_t i = 0; i < NUM_NETWORKS; ++i)
				{
					this->networks[i].active = false;
				}
			}
			this->networks[networkItem].active = networkActive;
			uint8_t SSIDlength = std::min(SSID_MAX_LENGTH, this->rx_buffer.at(4));
			uint8_t PASSlength = std::min(PASS_MAX_LENGTH, this->rx_buffer.at(5 + SSIDlength));
			this->networks[networkItem].ssid = std::string(this->rx_buffer.begin() + 5, this->rx_buffer.begin() + 5 + SSIDlength);
			this->networks[networkItem].pass = std::string(this->rx_buffer.begin() + 5 + SSIDlength + 1, this->rx_buffer.begin() + 5 + SSIDlength + 1 + PASSlength);

		}
		break;
	case toesp_cmds_t::NETWORK_UNREGISTER:
		UDBG("[Rainbow] ESP received command NETWORK_UNREGISTER\n");
		if (message_size == 2)
		{
			uint8_t const networkItem = this->rx_buffer.at(2);
			if (networkItem > NUM_NETWORKS - 1)
				break;
			this->networks[networkItem].ssid = "";
			this->networks[networkItem].pass = "";
			this->networks[networkItem].active = false;
		}
		break;
	case toesp_cmds_t::NETWORK_SET_ACTIVE:
		UDBG("[Rainbow] ESP received command NETWORK_SET_ACTIVE: ");
		if (message_size == 3)
		{
			uint8_t const networkItem = this->rx_buffer.at(2);
			if (networkItem > NUM_NETWORKS - 1)
				break;
			bool const networkActive = this->rx_buffer.at(3) == 0 ? false : true;
			UDBG("[Rainbow] ESP network %d (%s)\n", networkItem, networkActive ? "active" : "inactive");
			if (this->networks[networkItem].ssid == "")
				break;
			if (networkActive)
			{
				for (size_t i = 0; i < NUM_NETWORKS; ++i)
				{
					this->networks[i].active = false;
				}
			}
			this->networks[networkItem].active = networkActive;
		}
		break;

		// FILE CMDS

	case toesp_cmds_t::FILE_OPEN:
	{
		UDBG("[Rainbow] ESP received command FILE_OPEN\n");
		if (message_size >= 4)
		{
			uint8_t config = this->rx_buffer.at(2);
			FileConfig file_config = parseFileConfig(config);
			std::string filename;

			if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
			{
				uint8_t const path = this->rx_buffer.at(3);
				uint8_t const file = this->rx_buffer.at(4);
				if (path < NUM_FILE_PATHS && file < NUM_FILES)
				{
					filename = getAutoFilename(path, file);
					int i = findFile(file_config.drive, filename);
					if (i == -1)
					{
						FileStruct temp_file = {file_config.drive, filename, std::vector<uint8_t>()};
						this->files.push_back(temp_file);
					}
				}
			}
			else if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_MANUAL))
			{
				uint8_t const path_length = this->rx_buffer.at(3);
				filename = std::string(this->rx_buffer.begin() + 4, this->rx_buffer.begin() + 4 + path_length);
				int i = findFile(file_config.drive, filename);
				if (i == -1)
				{
					FileStruct temp_file = {file_config.drive, filename, std::vector<uint8_t>()};
					this->files.push_back(temp_file);
				}
			}
			int i = findFile(file_config.drive, filename);
			this->working_file.active = true;
			this->working_file.offset = 0;
			this->working_file.file = &this->files.at(i);
			this->saveFiles();
		}
		break;
	}
	case toesp_cmds_t::FILE_CLOSE:
		UDBG("[Rainbow] ESP received command FILE_CLOSE\n");
		this->working_file.active = false;
		this->saveFiles();
		break;
	case toesp_cmds_t::FILE_STATUS:
	{
		UDBG("[Rainbow] ESP received command FILE_STATUS\n");

		if (this->working_file.active == false)
		{
			this->tx_messages.push_back({
				2,
				static_cast<uint8_t>(fromesp_cmds_t::FILE_STATUS),
				0
			});
		}
		else
		{
			FileConfig file_config = parseFileConfig(this->working_file.config);
			if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
			{
				this->tx_messages.push_back({
					5,
					static_cast<uint8_t>(fromesp_cmds_t::FILE_STATUS),
					1,
					static_cast<uint8_t>(this->working_file.config),
					static_cast<uint8_t>(this->working_file.auto_path),
					static_cast<uint8_t>(this->working_file.auto_file),
				});
			}
			else if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_MANUAL))
			{
				std::string filename = this->working_file.file->filename;
				filename = filename.substr(filename.find_first_of("/") + 1);
				std::deque<uint8_t> message({
					static_cast<uint8_t>(3 + filename.size()),
					static_cast<uint8_t>(fromesp_cmds_t::FILE_STATUS),
					1,
					static_cast<uint8_t>(filename.size()),
				});
				message.insert(message.end(), filename.begin(), filename.end());
				this->tx_messages.push_back(message);
			}
		}
		break;
	}
	case toesp_cmds_t::FILE_EXISTS:
	{
		UDBG("[Rainbow] ESP received command FILE_EXISTS\n");

		if (message_size < 2)
		{
			break;
		}
		uint8_t config = this->rx_buffer.at(2);
		FileConfig file_config = parseFileConfig(config);
		std::string filename;
		int i = -1;

		if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
		{
			if (message_size == 4)
			{
				uint8_t const path = this->rx_buffer.at(3);
				uint8_t const file = this->rx_buffer.at(4);
				filename = getAutoFilename(path, file);
			}
		}
		else if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_MANUAL))
		{
			uint8_t const path_length = this->rx_buffer.at(3);
			filename = std::string(this->rx_buffer.begin() + 4, this->rx_buffer.begin() + 4 + path_length);
		}

		// special case just for emulation
		if (filename != "/web/")
		{
			if (filename.find_last_of("/") == filename.length() - 1)
			{
				i = findPath(file_config.drive, filename);
			}
			else
			{
				i = findFile(file_config.drive, filename);
			}
		}

		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::FILE_EXISTS),
			static_cast<uint8_t>(i == -1 ? 0 : 1)
		});
		break;
	}
	case toesp_cmds_t::FILE_DELETE:
	{
		UDBG("[Rainbow] ESP received command FILE_DELETE\n");

		if (message_size < 2)
		{
			break;
		}
		uint8_t config = this->rx_buffer.at(2);
		FileConfig file_config = parseFileConfig(config);
		std::string filename;
		int i = -1;

		if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
		{
			if (message_size == 4)
			{
				uint8_t const path = this->rx_buffer.at(3);
				uint8_t const file = this->rx_buffer.at(4);
				if (path < NUM_FILE_PATHS && file < NUM_FILES)
				{
					filename = getAutoFilename(path, file);
				}
				else
				{
					// Invalid path or file
					this->tx_messages.push_back({
						2,
						static_cast<uint8_t>(fromesp_cmds_t::FILE_DELETE),
						static_cast<uint8_t>(file_delete_results_t::INVALID_PATH_OR_FILE)
					});
					break;
				}
			}
		}
		else if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_MANUAL))
		{
			uint8_t const path_length = this->rx_buffer.at(4);
			filename = std::string(this->rx_buffer.begin() + 5, this->rx_buffer.begin() + 5 + path_length);
		}

		i = findFile(file_config.drive, filename);
		if (i == -1)
		{
			// File does not exist
			this->tx_messages.push_back({
				2,
				static_cast<uint8_t>(fromesp_cmds_t::FILE_DELETE),
				static_cast<uint8_t>(file_delete_results_t::FILE_NOT_FOUND)
			});
			break;
		}
		else
		{
			this->files.erase(this->files.begin() + i);
			this->saveFiles();
		}

		this->tx_messages.push_back({
			2,
			static_cast<uint8_t>(fromesp_cmds_t::FILE_DELETE),
			static_cast<uint8_t>(file_delete_results_t::SUCCESS)
		});

		break;
	}
	case toesp_cmds_t::FILE_SET_CUR:
		UDBG("[Rainbow] ESP received command FILE_SET_CUR\n");
		if (message_size >= 2 && message_size <= 5)
		{
			if (this->working_file.active)
			{
			this->working_file.offset = this->rx_buffer.at(2);
			if(message_size == 3) this->working_file.offset += this->rx_buffer.at(3) << 8;
			if(message_size == 4) this->working_file.offset += this->rx_buffer.at(4) << 16;
			if(message_size == 5) this->working_file.offset += this->rx_buffer.at(5) << 24;
			}
		}
		break;
	case toesp_cmds_t::FILE_READ:
		UDBG("[Rainbow] ESP received command FILE_READ\n");
		if (message_size == 2)
		{
			if (this->working_file.active)
			{
				uint8_t const n = this->rx_buffer.at(2);
				this->readFile(n);
				this->working_file.offset += n;
				UDBG("[Rainbow] ESP working file offset: %u (%x)\n", this->working_file.offset, this->working_file.offset);
				/*UDBG("file size: %lu bytes\n", this->esp_files[this->working_path_auto][this->working_file_auto].size());
				if (this->working_file.offset > this->esp_files[this->working_path_auto][this->working_file_auto].size()) {
					this->working_file.offset = this->esp_files[this->working_path_auto][this->working_file_auto].size();
				}*/
			}
			else
			{
				this->tx_messages.push_back({2, static_cast<uint8_t>(fromesp_cmds_t::FILE_DATA), 0});
			}
		}
		break;
	case toesp_cmds_t::FILE_WRITE:
		UDBG("[Rainbow] ESP received command FILE_WRITE\n");
		if (message_size >= 2 && this->working_file.active)
		{
			this->writeFile(this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
			this->working_file.offset += message_size - 1;
		}
		break;
	case toesp_cmds_t::FILE_APPEND:
		UDBG("[Rainbow] ESP received command FILE_APPEND\n");
		if (message_size >= 2 && this->working_file.active)
		{
			this->appendFile(this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
		}
		break;
	case toesp_cmds_t::FILE_COUNT:
	{
		UDBG("[Rainbow] ESP received command FILE_COUNT\n");

		if (message_size < 2)
		{
			break;
		}
		uint8_t config = this->rx_buffer.at(2);
		FileConfig file_config = parseFileConfig(config);

		if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
		{
			if (message_size == 3)
			{
				uint8_t const path = this->rx_buffer.at(3);
				if (path >= NUM_FILE_PATHS)
				{
					this->tx_messages.push_back({
						2,
						static_cast<uint8_t>(fromesp_cmds_t::FILE_COUNT),
						0
					});
				}
				else
				{
					uint8_t nb_files = 0;

					for (uint8_t file = 0; file < NUM_FILES; ++file)
					{
						std::string filename = getAutoFilename(path, file);
						int i = findFile(file_config.drive, filename);
						if (i != -1)
							nb_files++;
					}

					this->tx_messages.push_back({
						2,
						static_cast<uint8_t>(fromesp_cmds_t::FILE_COUNT),
						nb_files
					});
					UDBG("[Rainbow] ESP %u files found in path %u\n", nb_files, path);
				}
			}
		}
		else
		{
			// TODO manual mode
		}

		break;
	}
	case toesp_cmds_t::FILE_GET_LIST:
	{
		UDBG("[Rainbow] ESP received command FILE_GET_LIST\n");

		if (message_size < 2)
		{
			break;
		}
		uint8_t config = this->rx_buffer.at(2);
		FileConfig file_config = parseFileConfig(config);

		if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
		{
			if (message_size >= 3)
			{
				std::vector<uint8_t> existing_files;
				uint8_t const path = this->rx_buffer.at(3);
				uint8_t page_size = NUM_FILES;
				uint8_t current_page = 0;
				if (message_size == 5)
				{
					page_size = this->rx_buffer.at(4);
					current_page = this->rx_buffer.at(5);
				}
				uint8_t page_start = current_page * page_size;
				uint8_t page_end = current_page * page_size + page_size;
				uint8_t nb_files = 0;

				for (uint8_t file = 0; file < NUM_FILES; ++file)
				{
					std::string filename = getAutoFilename(path, file);
					int i = findFile(file_config.drive, filename);
					if (i != -1)
						nb_files++;
				}

				if (page_end > nb_files)
				{
					page_end = nb_files;
				}

				nb_files = 0;
				for (uint8_t file = 0; file < NUM_FILES; ++file)
				{
					std::string filename = getAutoFilename(path, file);
					int i = findFile(file_config.drive, filename);
					if (i != -1)
					{
						if (nb_files >= page_start && nb_files < page_end)
						{
							existing_files.push_back(file);
						}
						nb_files++;
					}
					if (nb_files >= page_end)
						break;
				}

				std::deque<uint8_t> message({
					static_cast<uint8_t>(existing_files.size() + 2),
					static_cast<uint8_t>(fromesp_cmds_t::FILE_LIST),
					static_cast<uint8_t>(existing_files.size())
				});
				message.insert(message.end(), existing_files.begin(), existing_files.end());
				this->tx_messages.push_back(message);
			}
		}
		else
		{
			// TODO manual mode
			UDBG("[Rainbow] ESP command FILE_GET_LIST manual mode not implemented\n");
			this->tx_messages.push_back({
				2,
				static_cast<uint8_t>(fromesp_cmds_t::FILE_LIST),
				0
			});
		}
		break;
	}
	case toesp_cmds_t::FILE_GET_FREE_ID:
		UDBG("[Rainbow] ESP received command FILE_GET_FREE_ID\n");
		if (message_size == 3)
		{
			uint8_t const drive = this->rx_buffer.at(2);
			uint8_t const path = this->rx_buffer.at(3);
			uint8_t i;

			for (i = 0; i < NUM_FILES; ++i)
			{
				std::string filename = getAutoFilename(path, i);
				int f = findFile(drive, filename);
				if (f == -1)
					break;
			}

			if (i != NUM_FILES)
			{
				// Free file ID found
				this->tx_messages.push_back({
					2,
					static_cast<uint8_t>(fromesp_cmds_t::FILE_ID),
					i,
				});
			}
			else
			{
				// Free file ID not found
				this->tx_messages.push_back({
					1,
					static_cast<uint8_t>(fromesp_cmds_t::FILE_ID)
				});
			}
		}
		break;
	case toesp_cmds_t::FILE_GET_FS_INFO:
	{
		UDBG("[Rainbow] ESP received command FILE_GET_FS_INFO\n");
		if (message_size < 2)
		{
			break;
		}
		uint8_t config = this->rx_buffer.at(2);
		FileConfig file_config = parseFileConfig(config);
		uint64_t free = 0;
		uint64_t used = 0;
		uint8_t free_pct = 0;
		uint8_t used_pct = 0;
		if (file_config.drive == static_cast<uint8_t>(file_config_flags_t::DESTINATION_ESP))
		{

			for (size_t i = 0; i < this->files.size(); ++i)
			{
				if (this->files.at(i).drive == static_cast<uint8_t>(file_config_flags_t::DESTINATION_ESP))
				{
					used += this->files.at(i).data.size();
				}
			}

			free = ESP_FLASH_SIZE - used;
			free_pct = ((ESP_FLASH_SIZE - used) * 100) / ESP_FLASH_SIZE;
			used_pct = 100 - free_pct; // (used * 100) / ESP_FLASH_SIZE;

			this->tx_messages.push_back({
				27,
				static_cast<uint8_t>(fromesp_cmds_t::FILE_FS_INFO),
				(ESP_FLASH_SIZE >> 54) & 0xff,
				(ESP_FLASH_SIZE >> 48) & 0xff,
				(ESP_FLASH_SIZE >> 40) & 0xff,
				(ESP_FLASH_SIZE >> 32) & 0xff,
				(ESP_FLASH_SIZE >> 24) & 0xff,
				(ESP_FLASH_SIZE >> 16) & 0xff,
				(ESP_FLASH_SIZE >> 8) & 0xff,
				(ESP_FLASH_SIZE)&0xff,
				static_cast<uint8_t>((free >> 54) & 0xff),
				static_cast<uint8_t>((free >> 48) & 0xff),
				static_cast<uint8_t>((free >> 40) & 0xff),
				static_cast<uint8_t>((free >> 32) & 0xff),
				static_cast<uint8_t>((free >> 24) & 0xff),
				static_cast<uint8_t>((free >> 16) & 0xff),
				static_cast<uint8_t>((free >> 8) & 0xff),
				static_cast<uint8_t>((free)&0xff),
				free_pct,
				static_cast<uint8_t>((used >> 54) & 0xff),
				static_cast<uint8_t>((used >> 48) & 0xff),
				static_cast<uint8_t>((used >> 40) & 0xff),
				static_cast<uint8_t>((used >> 32) & 0xff),
				static_cast<uint8_t>((used >> 24) & 0xff),
				static_cast<uint8_t>((used >> 16) & 0xff),
				static_cast<uint8_t>((used >> 8) & 0xff),
				static_cast<uint8_t>((used)&0xff),
				used_pct
			});
			break;
		}
		else if (file_config.drive == static_cast<uint8_t>(file_config_flags_t::DESTINATION_SD))
		{
			if (isSdCardFilePresent)
			{

				for (size_t i = 0; i < this->files.size(); ++i)
				{
					if (this->files.at(i).drive == static_cast<uint8_t>(file_config_flags_t::DESTINATION_SD))
					{
						used += this->files.at(i).data.size();
					}
				}

				free = SD_CARD_SIZE - used;
				free_pct = ((SD_CARD_SIZE - used) * 100) / SD_CARD_SIZE;
				used_pct = 100 - free_pct; // (used * 100) / SD_CARD_SIZE;

				this->tx_messages.push_back({
					27,
					static_cast<uint8_t>(fromesp_cmds_t::FILE_FS_INFO),
					(SD_CARD_SIZE >> 54) & 0xff,
					(SD_CARD_SIZE >> 48) & 0xff,
					(SD_CARD_SIZE >> 40) & 0xff,
					(SD_CARD_SIZE >> 32) & 0xff,
					(SD_CARD_SIZE >> 24) & 0xff,
					(SD_CARD_SIZE >> 16) & 0xff,
					(SD_CARD_SIZE >> 8) & 0xff,
					(SD_CARD_SIZE)&0xff,
					static_cast<uint8_t>((free >> 54) & 0xff),
					static_cast<uint8_t>((free >> 48) & 0xff),
					static_cast<uint8_t>((free >> 40) & 0xff),
					static_cast<uint8_t>((free >> 32) & 0xff),
					static_cast<uint8_t>((free >> 24) & 0xff),
					static_cast<uint8_t>((free >> 16) & 0xff),
					static_cast<uint8_t>((free >> 8) & 0xff),
					static_cast<uint8_t>((free)&0xff),
					free_pct,
					static_cast<uint8_t>((used >> 54) & 0xff),
					static_cast<uint8_t>((used >> 48) & 0xff),
					static_cast<uint8_t>((used >> 40) & 0xff),
					static_cast<uint8_t>((used >> 32) & 0xff),
					static_cast<uint8_t>((used >> 24) & 0xff),
					static_cast<uint8_t>((used >> 16) & 0xff),
					static_cast<uint8_t>((used >> 8) & 0xff),
					static_cast<uint8_t>((used)&0xff),
					used_pct
				});
				break;
			}
		}
		this->tx_messages.push_back({
			1,
			static_cast<uint8_t>(fromesp_cmds_t::FILE_FS_INFO)
		});
		break;
	}
	case toesp_cmds_t::FILE_GET_INFO:
	{
		UDBG("[Rainbow] ESP received command FILE_GET_INFO\n");

		if (message_size < 2)
		{
			break;
		}
		uint8_t config = this->rx_buffer.at(2);
		FileConfig file_config = parseFileConfig(config);

		if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
		{
			if (message_size == 4)
			{
				uint8_t const path = this->rx_buffer.at(3);
				uint8_t const file = this->rx_buffer.at(4);
				std::string filename = getAutoFilename(path, file);
				int i = findFile(file_config.drive, filename);
				if (path < NUM_FILE_PATHS && file < NUM_FILES && i != -1)
				{
					// Compute info
					uint32_t file_crc32;
					file_crc32 = CalcCRC32(0L, this->files.at(i).data.data(), this->files.at(i).data.size());
					size_t file_size = this->files.at(i).data.size();

					// Send info
					this->tx_messages.push_back({
						9,
						static_cast<uint8_t>(fromesp_cmds_t::FILE_INFO),

						static_cast<uint8_t>((file_crc32 >> 24) & 0xff),
						static_cast<uint8_t>((file_crc32 >> 16) & 0xff),
						static_cast<uint8_t>((file_crc32 >> 8) & 0xff),
						static_cast<uint8_t>(file_crc32 & 0xff),

						static_cast<uint8_t>((file_size >> 24) & 0xff),
						static_cast<uint8_t>((file_size >> 16) & 0xff),
						static_cast<uint8_t>((file_size >> 8) & 0xff),
						static_cast<uint8_t>(file_size & 0xff)
					});
				}
				else
				{
					// File not found or path/file out of bounds
					this->tx_messages.push_back({
						1,
						static_cast<uint8_t>(fromesp_cmds_t::FILE_INFO)
					});
				}
			}
		}
		else
		{
			// TODO manual mode
			UDBG("[Rainbow] ESP command FILE_GET_INFO manual mode not implemented\n");
		}

		break;
	}
	case toesp_cmds_t::FILE_DOWNLOAD:
	{
		UDBG("[Rainbow] ESP received command FILE_DOWNLOAD\n");

		if (message_size < 2)
		{
			break;
		}
		uint8_t config = this->rx_buffer.at(2);
		FileConfig file_config = parseFileConfig(config);

		if (file_config.access_mode == static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_AUTO))
		{
			if (message_size > 6)
			{
				// Parse
				uint8_t const urlLength = this->rx_buffer.at(3);
				if (message_size != urlLength + 5)
				{
					break;
				}
				std::string const url(this->rx_buffer.begin() + 4, this->rx_buffer.begin() + 4 + urlLength);

				uint8_t const path = this->rx_buffer.at(4 + urlLength);
				uint8_t const file = this->rx_buffer.at(4 + 1 + urlLength);

				// Delete existing file
				if (path < NUM_FILE_PATHS && file < NUM_FILES)
				{
					std::string filename = getAutoFilename(path, file);
					int i = findFile(file_config.drive, filename);
					if (i != -1)
					{
						this->files.erase(this->files.begin() + i);
						this->saveFiles();
					}
				}
				else
				{
					// Invalid path / file
					this->tx_messages.push_back({
						4,
						static_cast<uint8_t>(fromesp_cmds_t::FILE_DOWNLOAD),
						static_cast<uint8_t>(file_download_results_t::INVALID_DESTINATION),
						0,
						0
					});
					break;
				}

				// Download new file
				this->downloadFile(url, path, file);
			}
		}
		else
		{
			// TODO manual mode
			UDBG("[Rainbow] ESP command FILE_DOWNLOAD manual mode not implemented\n");
		}
		break;
	}
	case toesp_cmds_t::FILE_FORMAT:
		UDBG("[Rainbow] ESP received command FILE_FORMAT\n");
		if (message_size == 2)
		{
			uint8_t drive = rx_buffer.at(2);
			clearFiles(drive);
		}
		break;
	default:
		UDBG("[Rainbow] ESP received unknown message %02x\n", this->rx_buffer.at(1));
		break;
	};

	// Remove processed message
	this->rx_buffer.clear();
}

FileConfig BrokeStudioFirmware::parseFileConfig(uint8_t config)
{
	return FileConfig({
		static_cast<uint8_t>(config & static_cast<uint8_t>(file_config_flags_t::ACCESS_MODE_MASK)),
		static_cast<uint8_t>((config & static_cast<uint8_t>(file_config_flags_t::DESTINATION_MASK)))
	});
}

int BrokeStudioFirmware::findFile(uint8_t drive, std::string filename)
{
	for (size_t i = 0; i < this->files.size(); ++i)
	{
		if ((this->files.at(i).drive == drive) && (this->files.at(i).filename == filename))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

int BrokeStudioFirmware::findPath(uint8_t drive, std::string path)
{
	for (size_t i = 0; i < this->files.size(); ++i)
	{
		if ((this->files.at(i).drive == drive) && (this->files.at(i).filename.substr(0, path.length()) == path))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

std::string BrokeStudioFirmware::getAutoFilename(uint8_t path, uint8_t file)
{
	return "/" + dir_names[path] + "/file" + std::to_string(file) + ".bin";
}

void BrokeStudioFirmware::readFile(uint8_t n)
{
	// Get data range
	std::vector<uint8_t>::const_iterator data_begin;
	std::vector<uint8_t>::const_iterator data_end;
	if (this->working_file.offset >= this->working_file.file->data.size())
	{
		data_begin = this->working_file.file->data.end();
		data_end = data_begin;
	}
	else
	{
		data_begin = this->working_file.file->data.begin() + this->working_file.offset;
		data_end = this->working_file.file->data.begin() + std::min(static_cast<std::vector<uint8_t>::size_type>(this->working_file.offset) + n, this->working_file.file->data.size());
	}
	std::vector<uint8_t>::size_type const data_size = data_end - data_begin;

	// Write response
	std::deque<uint8_t> message({
		static_cast<uint8_t>(data_size + 2),
		static_cast<uint8_t>(fromesp_cmds_t::FILE_DATA),
		static_cast<uint8_t>(data_size)
	});
	message.insert(message.end(), data_begin, data_end);
	this->tx_messages.push_back(message);
}

template <class I>
void BrokeStudioFirmware::writeFile(I data_begin, I data_end)
{
	if (this->working_file.active == false)
	{
		return;
	}

	auto const data_size = data_end - data_begin;
	uint32_t const offset_end = this->working_file.offset + data_size;
	if (offset_end > this->working_file.file->data.size())
	{
		this->working_file.file->data.resize(offset_end, 0);
	}

	for (std::vector<uint8_t>::size_type i = this->working_file.offset; i < offset_end; ++i)
	{
		this->working_file.file->data[i] = *data_begin;
		++data_begin;
	}
}

template<class I>
void BrokeStudioFirmware::appendFile(I data_begin, I data_end)
{
	if (this->working_file.active == false) {
		return;
	}

	auto const data_size = data_end - data_begin;
	size_t file_size = this->working_file.file->data.size();
	uint32_t const offset_end = file_size + data_size;
	this->working_file.file->data.resize(offset_end, 0);

	for (vector<uint8_t>::size_type i = file_size; i < offset_end; ++i) {
		this->working_file.file->data[i] = *data_begin;
		++data_begin;
	}
}

void BrokeStudioFirmware::saveFiles()
{
	char const *esp_filesystem_file_path = ::getenv("RAINBOW_ESP_FILESYSTEM_FILE");
	if (esp_filesystem_file_path == NULL)
	{
		FCEU_printf("[Rainbow] RAINBOW_ESP_FILESYSTEM_FILE environment variable is not set\n");
	}
	else
	{
		saveFile(0, esp_filesystem_file_path);
	}

	char const *sd_filesystem_file_path = ::getenv("RAINBOW_SD_FILESYSTEM_FILE");
	if (sd_filesystem_file_path == NULL)
	{
		FCEU_printf("[Rainbow] RAINBOW_SD_FILESYSTEM_FILE environment variable is not set\n");
	}
	else
	{
		saveFile(2, sd_filesystem_file_path);
	}
}

void BrokeStudioFirmware::saveFile(uint8_t drive, char const *filename)
{
	std::ofstream ofs(filename, std::fstream::binary);
	if (ofs.fail())
	{
		FCEU_printf("[Rainbow] Couldn't open RAINBOW_FILESYSTEM_FILE (%s)\n", filename);
		return;
	}

	// header
	ofs << 'R';
	ofs << 'N';
	ofs << 'B';
	ofs << 'W';
	ofs << 'F';
	ofs << 'S';
	ofs << (char)0x1A;

	// file format version
	ofs << (char)(0x00);

	for (auto file = this->files.begin(); file != this->files.end(); ++file)
	{
		if (file->drive != drive)
			continue;

		// file separator
		ofs << 'F';
		ofs << '>';

		// filename length
		ofs << (char)file->filename.length();

		// filename
		for (char &c : std::string(file->filename))
		{
			ofs << (c);
		}
		// data size
		size_t size = file->data.size();
		ofs << (char)((size & 0xff000000) >> 24);
		ofs << (char)((size & 0x00ff0000) >> 16);
		ofs << (char)((size & 0x0000ff00) >> 8);
		ofs << (char)((size & 0x000000ff));

		// actual data
		for (uint8_t byte : file->data)
		{
			ofs << (char)byte;
		}
	}
}

void BrokeStudioFirmware::loadFiles()
{
	char const *esp_filesystem_file_path = ::getenv("RAINBOW_ESP_FILESYSTEM_FILE");
	if (esp_filesystem_file_path == NULL)
	{
		isEspFlashFilePresent = false;
		FCEU_printf("RAINBOW_ESP_FILESYSTEM_FILE environment variable is not set\n");
	}
	else
	{
		isEspFlashFilePresent = true;
		loadFile(0, esp_filesystem_file_path);
	}

	char const *sd_filesystem_file_path = ::getenv("RAINBOW_SD_FILESYSTEM_FILE");
	if (sd_filesystem_file_path == NULL)
	{
		isSdCardFilePresent = false;
		FCEU_printf("RAINBOW_SD_FILESYSTEM_FILE environment variable is not set\n");
	}
	else
	{
		isSdCardFilePresent = true;
		loadFile(2, sd_filesystem_file_path);
	}
}

void BrokeStudioFirmware::loadFile(uint8_t drive, char const *filename)
{
	std::ifstream ifs(filename, std::fstream::binary);
	if (ifs.fail())
	{
		FCEU_printf("Couldn't open RAINBOW_FILESYSTEM_FILE (%s)\n", filename);
		return;
	}

	clearFiles(drive);

	uint8_t l;
	uint8_t t;
	uint32_t size;
	uint8_t v;

	// check file header
	std::string header;
	header.push_back(ifs.get()); // R
	header.push_back(ifs.get()); // N
	header.push_back(ifs.get()); // B
	header.push_back(ifs.get()); // W
	header.push_back(ifs.get()); // F
	header.push_back(ifs.get()); // S
	header.push_back(ifs.get()); // 0x1A
	if (header != "RNBWFS\x1a")
	{
		FCEU_printf("RAINBOW_FILESYSTEM_FILE (%s) file invalid\n", filename);
		return;
	}

	// file format version
	v = ifs.get();

	if (v == 0)
	{
		while (ifs.peek() != EOF)
		{

			// check file separator
			header = "";
			header.push_back(ifs.get()); // F
			header.push_back(ifs.get()); //>
			if (header != "F>")
			{
				FCEU_printf("RAINBOW_FILESYSTEM_FILE (%s) file malformed\n", filename);
				return;
			}

			FileStruct temp_file;

			// drive
			temp_file.drive = drive;

			// filename length
			l = ifs.get();
			temp_file.filename.reserve(l);

			// filename
			for (size_t i = 0; i < l; ++i)
			{
				temp_file.filename.push_back(ifs.get());
			}

			// data size
			size = 0;
			t = ifs.get();
			size |= (t << 24);
			t = ifs.get();
			size |= (t << 16);
			t = ifs.get();
			size |= (t << 8);
			t = ifs.get();
			size |= t;
			temp_file.data.clear();
			temp_file.data.reserve(size);

			// actual data
			for (uint32_t i = 0; i < size; ++i)
			{
				t = ifs.get();
				temp_file.data.push_back(t);
			}
			this->files.push_back(temp_file);
		}
	}
	else
	{
		FCEU_printf("RAINBOW_FILESYSTEM_FILE (%s) format version unknown\n", filename);
	}
}

void BrokeStudioFirmware::clearFiles(uint8_t drive)
{
	unsigned int i = 0;
	while (i < this->files.size())
	{
		if (this->files.at(i).drive == drive)
		{
			this->files.erase(this->files.begin() + i);
		}
		else
		{
			++i;
		}
	}
}

template<class I>
void BrokeStudioFirmware::sendUdpDatagramToServer(I begin, I end)
{
#if RAINBOW_DEBUG_ESP >= 1
	FCEU_printf("RAINBOW %lu udp datagram to send", wall_clock_milli());
#	if RAINBOW_DEBUG_ESP >= 2
	FCEU_printf(": ");
	for (I cur = begin; cur < end; ++cur)
	{
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
		for (let i = 0; i < $1; ++i)
		{
			copied_array[i] = getValue($0+i, "i8");
		}
		FCEM.udpSend($2, copied_array);
	}, copied.data(), copied.size(), this->udp_socket);
}

template <class I>
void BrokeStudioFirmware::sendTcpDataToServer([[maybe_unused]] I begin, [[maybe_unused]] I end)
{
#if RAINBOW_DEBUG_ESP >= 1
	FCEU_printf("RAINBOW %lu tcp data to send", wall_clock_milli());
#	if RAINBOW_DEBUG_ESP >= 2
	FCEU_printf(": ");
	for (I cur = begin; cur < end; ++cur)
	{
		FCEU_printf("%02x ", *cur);
	}
#	endif
	FCEU_printf("\n");
#endif

	// TODO
}

std::deque<uint8_t> BrokeStudioFirmware::read_socket(int socket)
{
	// TODO tcp
	if (socket != this->udp_socket)
	{
		return std::deque<uint8_t>();
	}

	size_t const MAX_MSG_SIZE = 254;
	static std::array<uint8_t, MAX_MSG_SIZE> incoming_buffer;
	int msg_len;

	// Fetch message from network implementation
	msg_len = EM_ASM_INT({
		// Get buffered message from server
		let raw_payload = FCEM.udpReceive($2);
		if (raw_payload === null)
		{
			return 0;
		}
		let payload = new Uint8Array(raw_payload);
		if (payload.length > $1)
		{
			console.error("ignoring message from server, too long: "+ payload.length +" bytes");
			return 0;
		}

		// Copy payload to C++ heap
		for (let i = 0; i < payload.length; ++i)
		{
			setValue($0+i, payload[i], "i8");
		}
		return payload.length;
	}, incoming_buffer.data(), incoming_buffer.size(), socket);

	// Return fetched message
	if (msg_len > 0)
	{
		std::deque<uint8_t> message({
			static_cast<uint8_t>(msg_len+1),
			static_cast<uint8_t>(fromesp_cmds_t::MESSAGE_FROM_SERVER)
		});
		message.insert(message.end(), incoming_buffer.begin(), incoming_buffer.begin() + msg_len);
		return message;
	}
	return std::deque<uint8_t>();
}

void BrokeStudioFirmware::receiveDataFromServer()
{
	// TCP
	if (this->tcp_socket != -1)
	{
		std::deque<uint8_t> message = read_socket(this->tcp_socket);
		if (!message.empty())
		{
			this->tx_messages.push_back(message);
		}
	}

	// UDP
	if (this->udp_socket != -1)
	{
		std::deque<uint8_t> message = read_socket(this->udp_socket);
		if (!message.empty())
		{
			this->tx_messages.push_back(message);
		}
	}
}

void BrokeStudioFirmware::closeConnection()
{
	// Close UDP socket
	EM_ASM({
		return FCEM.closeUdpSocket($0);
	}, this->udp_socket);
	this->udp_socket = -1;
 
	// TODO Close TCP socket
	this->tcp_socket = -1;
}

void BrokeStudioFirmware::openConnection()
{
	this->closeConnection();

	if (this->active_protocol == server_protocol_t::TCP)
	{
		// TODO
		UDBG("[Rainbow] TCP not yet implemented");
	}else if (this->active_protocol == server_protocol_t::TCP_SECURED)
	{
		// TODO
		UDBG("[Rainbow] TCP_SECURED not yet implemented");
	}else if (this->active_protocol == server_protocol_t::UDP)
	{
		this->udp_socket = EM_ASM_INT({
			return FCEM.createUdpSocket(UTF8ToString($0), $1);
		}, this->server_settings_address.c_str(), this->server_settings_port);
		UDBG("[Rainbow] got udp socket %d\n", this->udp_socket);
	}
}

void BrokeStudioFirmware::receivePingResult()
{
	uint32_t min, max, avg, lost;
	int ready = EM_ASM_INT({
		let result = FCEM.popPingResult();
		if (result === null)
		{
			return 0;
		}

		let avg = 0;
		if (result.lost < result.number)
		{
			avg = result.total / (result.number - result.lost);
		}

		setValue($0, result.min, "i32");
		setValue($1, result.max, "i32");
		setValue($2, avg, "i32");
		setValue($3, result.lost, "i32");
		return 1;
	}, &min, &max, &avg, &lost);

	if (ready)
	{
		auto esp_time = [](uint32_t ms) -> uint8
		{
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
			static_cast<uint8_t>(fromesp_cmds_t::SERVER_PING),
			esp_time(min),
			esp_time(max),
			esp_time(avg),
			static_cast<uint8_t>(lost)
		});
		this->ping_running = false;
	}
}

void BrokeStudioFirmware::initDownload()
{
	// TODO
}

void BrokeStudioFirmware::downloadFile([[maybe_unused]] std::string const& url, [[maybe_unused]] uint8_t path, [[maybe_unused]] uint8_t file)
{
	UDBG("[Rainbow] BrokeStudioFirmware download %s -> (%u,%u)\n", url.c_str(), (unsigned int)path, (unsigned int)file);
	// TODO
}

void BrokeStudioFirmware::cleanupDownload()
{
	// TODO
}
