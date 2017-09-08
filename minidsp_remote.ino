#include "mcp_can.h"

#define REMOTE_CAN_ADDR	(514)
#define DSP_CAN_ADDR	(513)

#define CMD_GET_STATUS	(131)
#define CMD_STATUS	(132)
#define CMD_SET_CONFIG	(138)
#define CMD_MUTE	(142)
#define CMD_GET_CONFIG	(145)
#define CMD_IR		(149)
#define CMD_VOLUME	(150)
#define CMD_START	(163)

unsigned char buf[8];

enum volume_dir {
	VOLUME_DOWN,
	VOLUME_UP,
};

/* текущий конфиг */
uint8_t cur_config = 0u;

/* init MCP1525, CS pin 10 */
MCP_CAN CAN(10);

void send_start_msg();
void get_minidsp_status();

void setup()
{
	Serial.begin(115200);

	while (CAN_OK != CAN.begin(CAN_500KBPS)) {
		Serial.println("CAN BUS Shield init fail");
		Serial.println("Init CAN BUS Shield again");
		delay(100);
	}

	Serial.println("CAN BUS Shield init ok!");

	send_start_msg();
	get_minidsp_status();
}

bool parse_msg(uint8_t * buf, uint8_t len)
{
	uint8_t i;
	uint8_t crc = 0u;

	/* 1. проверка CRC */
	for (i = 0; i < (len - 1u); i++) {
		crc += buf[i];
	}

	if (crc != buf[len - 1u]) {
		Serial.print("invalid msg crc! ");
		Serial.println(crc);

		return false;
	} else {
		//uint8_t m_len = buf[0];
		uint8_t m_type = buf[1];

		switch (m_type) {
		case CMD_STATUS:
			Serial.print("Attenuation: -");
			Serial.print(buf[2] >> 1);
			Serial.println("dB");

			Serial.print("Mute: ");
			Serial.println(buf[3]);

			Serial.print("Unknown: ");
			Serial.println(buf[4]);

			Serial.print("Config: ");
			Serial.println(buf[5]);

			cur_config = buf[5];

			break;

		case CMD_GET_CONFIG:
			Serial.print("Query config: ");
			Serial.println(buf[3]);

			cur_config = buf[3];

		default:
			Serial.println("Unknown msg type!");

			return false;
		}
	}

	return true;
}


void send_can_msg(uint8_t len, uint8_t * buf)
{
	uint8_t i;
	uint8_t crc;
	uint8_t send_buf[16];

	send_buf[0] = len + 1;
	crc = send_buf[0];
	for (i = 0; i < len; i++) {
		send_buf[i + 1] = buf[i];
		crc += buf[i];
	}
	send_buf[len + 1] = crc;

	CAN.sendMsgBuf(REMOTE_CAN_ADDR, 0, len + 2, (uint8_t *)send_buf);
}

void send_start_msg()
{
	uint8_t cmd_buf[1];

	cmd_buf[0] = CMD_START;
	send_can_msg(1, cmd_buf);
}

void get_minidsp_status()
{
	uint8_t cmd_buf[1];

	cmd_buf[0] = CMD_GET_STATUS;
	send_can_msg(1, cmd_buf);
}

void toggle_mute()
{
	uint8_t cmd_buf[1];

	cmd_buf[0] = CMD_MUTE;
	send_can_msg(1, cmd_buf);
}

void change_volume(enum volume_dir dir)
{
	uint8_t cmd_buf[2];

	cmd_buf[0] = CMD_VOLUME;
	cmd_buf[1] = (uint8_t)dir;

	send_can_msg(2, cmd_buf);
}

void send_set_cfg(uint8_t num)
{
	uint8_t cmd_buf[2];

	Serial.print("set CFG to ");
	Serial.print(num + 1);
	Serial.println();

	cmd_buf[0] = CMD_SET_CONFIG;
	cmd_buf[1] = num;

	send_can_msg(2, cmd_buf);

}

void loop()
{
	if (CAN_MSGAVAIL == CAN.checkReceive()) {
		uint8_t len = 0;

		CAN.readMsgBuf(&len, buf);

		if (parse_msg(buf, len) == false) {
			Serial.print("id: ");
			Serial.print(CAN.getCanId());
			Serial.print("; data: ");

			for (uint8_t i = 0; i < len; i++) {
				Serial.print(buf[i]);
				Serial.print("\t");
			}
			Serial.println();
		}
	}

	if (Serial.available() > 0) {
		int c;
		c = Serial.read();

		switch (c) {
		case '1':
			send_set_cfg(0);
			break;

		case '2':
			send_set_cfg(1);
			break;

		case '3':
			send_set_cfg(2);
			break;

		case '4':
			send_set_cfg(3);
			break;

		case '+':
			change_volume(VOLUME_UP);
			break;

		case '-':
			change_volume(VOLUME_DOWN);
			break;

		case 'g':
			get_minidsp_status();
			break;

		case 's':
			send_start_msg();
			break;

		case 'm':
			toggle_mute();
			break;
		}
	}
}

