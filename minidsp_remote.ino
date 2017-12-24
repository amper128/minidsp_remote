#include "Wire.h"
#include "mcp_can.h"

/* адрес ардуины на шине i2c */
#define SLAVE_I2C_ADDR (0x08)

/* CAN шина */
/* адрес ардуины в качестве пульта */
#define REMOTE_CAN_ADDR (514)
/* адрес miniDSP */
#define DSP_CAN_ADDR (513)

#define CMD_GET_STATUS (131)
#define CMD_STATUS (132)
#define CMD_SET_CONFIG (138)
#define CMD_MUTE (142)
#define CMD_GET_CONFIG (145)
#define CMD_IR (149)
#define CMD_VOLUME (150)
#define CMD_START (163)

static uint8_t vol_table[] = {
    0,  0xFF, 3,  6,    0xFF, 9,    12,   0xFF, 15,   18,   21,  0xFF, 24,   27, 30,   0xFF,
    33, 36,   39, 0xFF, 42,   45,   0xFF, 48,   51,   0xFF, 54,  57,   0xFF, 60, 63,   0xFF,
    66, 0xFF, 69, 0xFF, 72,   0xFF, 75,   78,   0xFF, 81,   84,  0xFF, 0xFF, 87, 0xFF, 0xFF,
    91, 0xFF, 94, 0xFF, 0xFF, 97,   0xFF, 0xFF, 101,  0xFF, 105, 0xFF, 255};

static unsigned char buf[8];

enum volume_dir {
	VOLUME_DOWN,
	VOLUME_UP,
};

/* init MCP1525, CS pin 10 */
static MCP_CAN CAN(10);

static void send_start_msg();
static void get_minidsp_status();
static void receiveEvent(int howMany);

static uint8_t n_volume = 0xFF;
static uint8_t dsp_volume = 0xFF;

static uint8_t n_mute = 0;
static uint8_t dsp_mute = 0;

static uint8_t n_input = 0;
static uint8_t dsp_input = 0;

void
setup()
{
	Wire.begin(SLAVE_I2C_ADDR);
	Wire.onReceive(receiveEvent);

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

void
receiveEvent(int)
{
	int i = 0;
	int vol_t;

	while (Wire.available()) {
		buf[i] = uint8_t(Wire.read() & 0xFF);
		i++;
	}

	vol_t = 255 - ((buf[1] - 1) & 0xFF);

	n_volume = vol_table[vol_t];
	n_input = buf[2];
	n_mute = buf[3];
}

bool
parse_msg(uint8_t *buf, uint8_t len)
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
		uint8_t m_type = buf[1];

		switch (m_type) {
		case CMD_STATUS:
			dsp_volume = buf[2];
			dsp_mute = buf[3];
			dsp_input = buf[5];

			break;

		case CMD_GET_CONFIG:
			Serial.print("Query config: ");
			Serial.println(buf[3]);

			dsp_input = buf[3];

		default:
			Serial.println("Unknown msg type!");

			return false;
		}
	}

	return true;
}

void
send_can_msg(uint8_t len, uint8_t *buf)
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

void
send_start_msg()
{
	uint8_t cmd_buf[1];

	cmd_buf[0] = CMD_START;
	send_can_msg(1, cmd_buf);
}

void
get_minidsp_status()
{
	uint8_t cmd_buf[1];

	cmd_buf[0] = CMD_GET_STATUS;
	send_can_msg(1, cmd_buf);
}

void
toggle_mute()
{
	uint8_t cmd_buf[1];

	cmd_buf[0] = CMD_MUTE;
	send_can_msg(1, cmd_buf);
}

void
change_volume(enum volume_dir dir)
{
	uint8_t cmd_buf[2];

	cmd_buf[0] = CMD_VOLUME;
	cmd_buf[1] = (uint8_t)dir;

	send_can_msg(2, cmd_buf);
}

void
send_set_cfg(uint8_t num)
{
	uint8_t cmd_buf[2];

	Serial.print("set CFG to ");
	Serial.print(num + 1);
	Serial.println();

	cmd_buf[0] = CMD_SET_CONFIG;
	cmd_buf[1] = num;

	send_can_msg(2, cmd_buf);
}

void
loop()
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

	if (n_mute != dsp_mute) {
		toggle_mute();
	}

	if (n_input != dsp_input) {
		send_set_cfg(n_input);
	}

	if (!n_mute) {
		if ((n_volume - dsp_volume) > 3) {
			change_volume(VOLUME_DOWN);
		} else if ((dsp_volume - n_volume) > 0) {
			change_volume(VOLUME_UP);
		}
	}

	delay(50);
}

