//#define DEBUG

#include <Wire.h>
#include "ds3231.h"
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <SPIFlash.h>

#ifdef DEBUG
#include <SoftwareSerial.h>
#endif

#define UART_SPEED 38400

//������ ��������, ����� ������� � ����
#define FW_VERSION						105

#ifdef DEBUG
#define DEBUG_RX						2 //
#define DEBUG_TX						6 //
#endif

#define BUZZER_PIN						3 //�������
#define LED_PIN							4 //��������� �����
#define RTC_ENABLE_PIN					5 //������� ����� ����� �����������

#define FLASH_ENABLE_PIN				7 //SPI enable pin
#define FLASH_SS_PIN					8 //SPI SELECT pin
#define RFID_RST_PIN					9 //���� ������ reset
#define RFID_SS_PIN						10 //���� ������ chip_select
//#define RFID_MOSI_PIN					11 //���� ������
//#define RFID_MISO_PIN					12 //���� ������
//#define RFID_SCK_PIN					13 //���� ������
#define BATTERY_PIN						A0 //����� ���������� �������
#define ERROR_LED_PIN					A1 //��������� ������ (�������)

//����� ������� � eeprom ������
#define EEPROM_STATION_NUMBER	00
//����� ������ � eeprom ������
#define EEPROM_STATION_MODE		10
//�����. ��������� �������� ADC � ������ = 0,00587
#define EEPROM_VOLTAGE_KOEFF	20
//�������� ������� RFID
#define EEPROM_GAIN				40
//��� ����, � ������� ������ �������� �������
#define EEPROM_CHIP_TYPE				50

//�������
#define COMMAND_SET_MODE			0x80
#define COMMAND_SET_TIME			0x81
#define COMMAND_RESET_STATION		0x82
#define COMMAND_GET_STATUS			0x83
#define COMMAND_INIT_CHIP			0x84
#define COMMAND_GET_LAST_TEAMS		0x85
#define COMMAND_GET_TEAM_RECORD		0x86
#define COMMAND_READ_CARD_PAGE		0x87
#define COMMAND_UPDATE_TEAM_MASK	0x88
#define COMMAND_WRITE_CARD_PAGE		0x89
#define COMMAND_READ_FLASH			0x8a
#define COMMAND_WRITE_FLASH			0x8b
#define COMMAND_ERASE_FLASH_SECTOR	0x8c
#define COMMAND_GET_CONFIG			0x8d
#define COMMAND_SET_KOEFF			0x8e
#define COMMAND_SET_GAIN			0x8f
#define COMMAND_SET_CHIP_TYPE		0x90

//������� ������ ��� ������
#define DATA_LENGTH_SET_MODE			1
#define DATA_LENGTH_SET_TIME			6
#define DATA_LENGTH_RESET_STATION		7
#define DATA_LENGTH_GET_STATUS			0
#define DATA_LENGTH_INIT_CHIP			4
#define DATA_LENGTH_GET_LAST_TEAMS		0
#define DATA_LENGTH_GET_TEAM_RECORD		2
#define DATA_LENGTH_READ_CARD_PAGE		2
#define DATA_LENGTH_UPDATE_TEAM_MASK	8
#define DATA_LENGTH_WRITE_CARD_PAGE		13
#define DATA_LENGTH_READ_FLASH			8
#define DATA_LENGTH_WRITE_FLASH			4  //and more according to data length
#define DATA_LENGTH_ERASE_FLASH_SECTOR	2
#define DATA_LENGTH_GET_CONFIG			0
#define DATA_LENGTH_SET_KOEFF			4
#define DATA_LENGTH_SET_GAIN			1
#define DATA_LENGTH_SET_CHIP_TYPE		1

//������ �������
#define REPLY_SET_MODE				0x90
#define REPLY_SET_TIME				0x91
#define REPLY_RESET_STATION			0x92
#define REPLY_GET_STATUS			0x93
#define REPLY_INIT_CHIP				0x94
#define REPLY_GET_LAST_TEAMS		0x95
#define REPLY_GET_TEAM_RECORD		0x96
#define REPLY_READ_CARD_PAGE		0x97
#define REPLY_UPDATE_TEAM_MASK		0x98
#define REPLY_WRITE_CARD_PAGE		0x99
#define REPLY_READ_FLASH			0x9a
#define REPLY_WRITE_FLASH			0x9b
#define REPLY_ERASE_FLASH_SECTOR	0x9c
#define REPLY_GET_CONFIG			0x9d
#define REPLY_SET_KOEFF				0x9e
#define REPLY_SET_GAIN				0x9f
#define REPLY_SET_CHIP_TYPE		0xa0

//������ �������
#define MODE_INIT		0
#define MODE_START_KP	1
#define MODE_FINISH_KP	2

//���� ������ �������
#define OK				0
#define WRONG_STATION	1
#define READ_ERROR		2
#define WRITE_ERROR		3
#define LOW_INIT_TIME	4
#define WRONG_CHIP		5
#define NO_CHIP			6
#define BUFFER_OVERFLOW	7
#define WRONG_DATA		8
#define WRONG_UID		9
#define WRONG_TEAM		10
#define NO_DATA			11
#define WRONG_COMMAND	12
#define ERASE_ERROR		13
#define WRONG_CHIP_TYPE	14
#define WRONG_MODE		15

//�������� � ����. 0-7 ���������, 8-... ��� �������
#define PAGE_UID		0
//�����_���� + ���_���� + ������_��������
#define PAGE_CHIP_NUM	4
//����� �������������
#define PAGE_INIT_TIME	5
#define PAGE_TEAM_MASK	6
#define PAGE_RESERVED2	7
#define PAGE_DATA_START	8

//��� ����
uint8_t chipType = 0x3e;
//������� ��� �����
uint8_t NTAG_MARK = 215;
//������ ���� � ���������
uint8_t TAG_MAX_PAGE = 130;


//������ ������ ���� (�� 1 ���)
#define LOG_RECORD_LENGTH	1024
#define ERASE_BLOCK_LENGTH 4096

uint32_t tmpBufferStart = 4 * 1024 * 1024 - ERASE_BLOCK_LENGTH;
//������������ ���-�� ������� � ����
uint32_t LOG_LENGTH = 4 * 1024 - ERASE_BLOCK_LENGTH / LOG_RECORD_LENGTH;

//�������� ���������
#define STATION_NUMBER_BYTE	3
#define LENGTH_BYTE			4
#define COMMAND_BYTE		5
#define DATA_START_BYTE		6

//����-��� ������ ������� � ������� ������
#define receiveTimeOut 1000

//������ ������ ��������� ������
const uint8_t lastTeamsLength = 10;

//������� ���������� ��������� ������� ����
uint8_t lastTeams[lastTeamsLength * 2];
uint32_t lastTimeChecked = 0;

// ���������� ���������� ����� � ������.
uint16_t totalChipsChecked = 0;

//�� ��������� ����� ������� � �����.
uint8_t stationNumber = 0;
uint8_t stationMode = MODE_INIT;
const uint32_t maxTimeInit = 600000UL; //���� ������

//�����. �������� �������� ��� � ���������� ��� �������� 10���/2.2���
float voltageCoeff = 0.00578;

uint8_t ntag_page[16]; //����� ��� ������ �� ����� ����� ntagRead4pages()

SPIFlash SPIflash(FLASH_SS_PIN); //����-������

//����-������
MFRC522::StatusCode status;
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
//�����. �������� ������� - �������� ������ ���� 4,5,6
byte gainCoeff = 0x70;

//�������� �������
struct ts systemTime;

//UART command buffer
uint8_t uartBuffer[256];
uint8_t uartBufferPosition = 0;
bool uartReady = false;
uint32_t uartTimeout = 1000;
bool receivingData = false;
uint32_t receiveStartTime = 0;

//����� ����� ��� ������ � ����
uint8_t newTeamMask[8];

#ifdef DEBUG
SoftwareSerial DebugSerial(DEBUG_RX, DEBUG_TX);
#endif

void(*resetFunc) (void) = 0; //declare reset function @ address 0

void setup()
{
	Serial.begin(UART_SPEED);
#ifdef DEBUG
	DebugSerial.begin(9600);
#endif

	analogReference(INTERNAL);

	pinMode(LED_PIN, OUTPUT);
	pinMode(ERROR_LED_PIN, OUTPUT);
	pinMode(BUZZER_PIN, OUTPUT);

	//���� ������� �� ����
	pinMode(FLASH_ENABLE_PIN, OUTPUT);
	digitalWrite(FLASH_ENABLE_PIN, HIGH);
	delay(1);
	SPIflash.begin();

	//���� ������� �� ����
	pinMode(RTC_ENABLE_PIN, OUTPUT);
	digitalWrite(RTC_ENABLE_PIN, HIGH);
	delay(1);
	DS3231_init(DS3231_INTCN);
	DS3231_get(&systemTime);

	//������ ����� ������� �� ������
	stationNumber = eepromread(EEPROM_STATION_NUMBER);
	if (stationNumber == 255 || stationNumber == -1)
	{
		stationNumber = 0;
	}

	//������ ����� ������ �� ������
	int c = eepromread(EEPROM_STATION_MODE);
	if (c == -1)
	{
		beep(200, 20);
	}
	if (c == MODE_START_KP) stationMode = MODE_START_KP;
	else if (c == MODE_FINISH_KP) stationMode = MODE_FINISH_KP;
	else stationMode = MODE_INIT;

	//������ �����. ��������� ����������
	union Convert
	{
		float number;
		byte byte[4];
	} p;
	byte flag = 0;
	for (byte i = 0; i < 4; i++)
	{
		p.byte[i] = eepromread(EEPROM_VOLTAGE_KOEFF + i * 3); //Read the station number from the EEPROM
		if (p.byte[i] == 0xff) flag++;
	}
	if (flag < 4) voltageCoeff = p.number;

	//������ �����. ��������
	gainCoeff = eepromread(EEPROM_GAIN);
	if (gainCoeff == 255 || gainCoeff == -1)
	{
		gainCoeff = 0x70;
	}

	//������ ��� ����
	chipType = eepromread(EEPROM_CHIP_TYPE);
	selectChipType(chipType);

	uint32_t flashSize = SPIflash.getCapacity();

	tmpBufferStart = flashSize - ERASE_BLOCK_LENGTH;
	LOG_LENGTH = (flashSize - ERASE_BLOCK_LENGTH) / LOG_RECORD_LENGTH;

	totalChipsChecked = refreshChipCounter();

	beep(800, 1);
}

void loop()
{
	/*
	 * � ����� ������� ���������� ������ �� �����, ���� �������� ������ �����,
	 * �� ���� �������������� ������� � ����������� �
	 * ���� ����� ������ �� �������, �� ������� �������� ����� ��� �� - ������������� ������ ������� �� ����
	 */

	 // check receive timeout
	if (receivingData && millis() - receiveStartTime > receiveTimeOut)
	{
#ifdef DEBUG
		//DebugSerial.println(F("receive timeout"));
#endif
		uartBufferPosition = 0;
		uartReady = false;
		receivingData = false;
		//errorBeepMs(50, 1);
	}

	// check UART for data
	if (Serial.available())
	{
		uartReady = readUart();
	}

	//���������� ��������� �������
	if (uartReady)
	{
		uartReady = false;
		executeCommand();
	}

	//���� ����� �� �� �������� ��� ���������
	if (stationMode != 0)
	{
		processRfidCard();
	}
}

//Commands processing

//����� �������
void executeCommand()
{
#ifdef DEBUG
	//DebugSerial.print(F("Command:"));
	//DebugSerial.println(String(uartBuffer[COMMAND_BYTE], HEX));
#endif
	bool errorLengthFlag = false;
	switch (uartBuffer[COMMAND_BYTE])
	{
	case COMMAND_SET_MODE:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_MODE) setMode();
		else errorLengthFlag = true;
		break;
	case COMMAND_SET_TIME:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_TIME) setTime();
		else errorLengthFlag = true;
		break;
	case COMMAND_RESET_STATION:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_RESET_STATION) resetStation();
		else errorLengthFlag = true;
		break;
	case COMMAND_GET_STATUS:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_STATUS) getStatus();
		else errorLengthFlag = true;
		break;
	case COMMAND_INIT_CHIP:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_INIT_CHIP) initChip();
		else errorLengthFlag = true;
		break;
	case COMMAND_GET_LAST_TEAMS:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_LAST_TEAMS) getLastTeams();
		else errorLengthFlag = true;
		break;
	case COMMAND_GET_TEAM_RECORD:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_TEAM_RECORD) getTeamRecord();
		else errorLengthFlag = true;
		break;
	case COMMAND_READ_CARD_PAGE:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_READ_CARD_PAGE) readCardPages();
		else errorLengthFlag = true;
		break;
	case COMMAND_UPDATE_TEAM_MASK:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_UPDATE_TEAM_MASK) updateTeamMask();
		else errorLengthFlag = true;
		break;
	case COMMAND_WRITE_CARD_PAGE:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_WRITE_CARD_PAGE) writeCardPage();
		else errorLengthFlag = true;
		break;
	case COMMAND_READ_FLASH:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_READ_FLASH) readFlash();
		else errorLengthFlag = true;
		break;
	case COMMAND_WRITE_FLASH:
		if (uartBuffer[LENGTH_BYTE] >= DATA_LENGTH_WRITE_FLASH) writeFlash();
		else errorLengthFlag = true;
		break;
	case COMMAND_ERASE_FLASH_SECTOR:
		if (uartBuffer[LENGTH_BYTE] >= DATA_LENGTH_ERASE_FLASH_SECTOR) eraseFlashSector();
		else errorLengthFlag = true;
		break;
	case COMMAND_GET_CONFIG:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_CONFIG) getConfig();
		else errorLengthFlag = true;
		break;
	case COMMAND_SET_KOEFF:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_KOEFF) setVCoeff();
		else errorLengthFlag = true;
		break;
	case COMMAND_SET_GAIN:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_GAIN) setGain();
		else errorLengthFlag = true;
		break;
	case COMMAND_SET_CHIP_TYPE:
		if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_CHIP_TYPE) setChipType();
		else errorLengthFlag = true;
		break;
	}

	uartBufferPosition = 0;
	if (errorLengthFlag) sendError(WRONG_COMMAND, 0);

#ifdef DEBUG
	//if (errorLengthFlag) DebugSerial.println(F("Incorrect data length"));
#endif
}

//��������� ������
void setMode()
{
	if (stationNumber == 0 || stationNumber == 0xff)
	{
		sendError(WRONG_MODE, REPLY_SET_MODE);
		return;
	}
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_SET_MODE);
		return;
	}

	//0: ����� ����� ������
	stationMode = uartBuffer[DATA_START_BYTE];
	if (!eepromwrite(EEPROM_STATION_MODE, stationMode)) errorBeep(1);

	//������������ ������ ������.
	init_package(REPLY_SET_MODE);

	//0: ��� ������
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_SET_MODE);
		return;
	}
	sendData();
}

//���������� ������� �� �������
void setTime()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_SET_MODE);
		return;
	}

	systemTime.year = uartBuffer[DATA_START_BYTE] + 2000;
	systemTime.mon = uartBuffer[DATA_START_BYTE + 1];
	systemTime.mday = uartBuffer[DATA_START_BYTE + 2];
	systemTime.hour = uartBuffer[DATA_START_BYTE + 3];
	systemTime.min = uartBuffer[DATA_START_BYTE + 4];
	systemTime.sec = uartBuffer[DATA_START_BYTE + 5];

#ifdef DEBUG
	//DebugSerial.print(F("Time: "));
	//for (uint8_t i = 0; i < 6; i++) DebugSerial.println(String(uartBuffer[DATA_START_BYTE + i]) + " ");
#endif

	//0-3: ���� � ����� � unixtime
	//systemTime.unixtime = uartBuffer[DATA_START_BYTE] * 16777216 + uartBuffer[DATA_START_BYTE] * 65536 + uartBuffer[DATA_START_BYTE] * 256 + uartBuffer[DATA_START_BYTE];

	delay(1);
	DS3231_set(systemTime); //correct time

	DS3231_get(&systemTime);
	uint32_t tmpTime = systemTime.unixtime;

	init_package(REPLY_SET_TIME);

	//0: ��� ������
	//1-4: ������� �����
	bool flag = true;
	flag &= addData(OK);
	flag &= addData((tmpTime & 0xFF000000) >> 24);
	flag &= addData((tmpTime & 0x00FF0000) >> 16);
	flag &= addData((tmpTime & 0x0000FF00) >> 8);
	flag &= addData(tmpTime & 0x000000FF);
	if (!flag)
	{
		sendError(BUFFER_OVERFLOW, REPLY_SET_TIME);
		return;
	}
	sendData();
}

//����� �������� �������
void resetStation()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_RESET_STATION);
		return;
	}

	//0-1: ���-�� ���������� ���� (��� ������)
	//2-5: ����� ��������� �������(��� ������)
	//6 : ����� ����� �������
	//��������� ���������� �������
	uint16_t checkCardNumber = uartBuffer[DATA_START_BYTE];
	checkCardNumber <<= 8;
	checkCardNumber += uartBuffer[DATA_START_BYTE + 1];

	if (checkCardNumber != totalChipsChecked)
	{
		sendError(WRONG_DATA, REPLY_RESET_STATION);
		return;
	}

	//��������� ����� ��������� �������
	uint32_t checkLastTime = uartBuffer[DATA_START_BYTE + 2];
	checkLastTime <<= 8;
	checkLastTime += uartBuffer[DATA_START_BYTE + 3];
	checkLastTime <<= 8;
	checkLastTime += uartBuffer[DATA_START_BYTE + 4];
	checkLastTime <<= 8;
	checkLastTime += uartBuffer[DATA_START_BYTE + 5];
	if (checkLastTime != lastTimeChecked)
	{
		sendError(WRONG_DATA, REPLY_RESET_STATION);
		return;
	}

	stationMode = 0;
	if (!eepromwrite(EEPROM_STATION_MODE, stationMode)) errorBeep(1);
	stationNumber = uartBuffer[DATA_START_BYTE + 6];
	if (!eepromwrite(EEPROM_STATION_NUMBER, stationNumber)) errorBeep(1);

	SPIflash.eraseChip();

	lastTimeChecked = 0;
	totalChipsChecked = 0;

	init_package(REPLY_RESET_STATION);

	//0: ��� ������
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_RESET_STATION);
		return;
	}

	sendData();
	delay(100);
	resetFunc();
}

// ������ ������: ����� �� �������, ����� �������, ����� ������, ����� �������, ����� ��������� ��������
void getStatus()
{
	DS3231_get(&systemTime);
	uint32_t tmpTime = systemTime.unixtime;

	//0: ��� ������
	//1 - 4: ������� �����
	//5 - 6 : ���������� ������� �� �������
	//7 - 10 : ����� ��������� ������� �� �������
	//11 - 12 : ���������� ������� � �������� ��������[0..1023] ~[0..1.1�]
	//13 - 14 : ����������� ���� DS3231(���� ���� ���������� �����)
	init_package(REPLY_GET_STATUS);

	bool flag = true;
	flag &= addData(OK);

	flag &= addData((tmpTime & 0xFF000000) >> 24);
	flag &= addData((tmpTime & 0x00FF0000) >> 16);
	flag &= addData((tmpTime & 0x0000FF00) >> 8);
	flag &= addData(tmpTime & 0x000000FF);

	flag &= addData((totalChipsChecked & 0xFF00) >> 8);
	flag &= addData(totalChipsChecked & 0x00FF);

	flag &= addData((lastTimeChecked & 0xFF000000) >> 24);
	flag &= addData((lastTimeChecked & 0x00FF0000) >> 16);
	flag &= addData((lastTimeChecked & 0x0000FF00) >> 8);
	flag &= addData(lastTimeChecked & 0x000000FF);

	uint16_t batteryLevel = getBatteryLevel();
	flag &= addData((batteryLevel & 0xFF00) >> 8);
	flag &= addData(batteryLevel & 0x00FF);

	int temperature = (int)DS3231_get_treg();
	flag &= addData((temperature & 0xFF00) >> 8);
	flag &= addData(temperature & 0x00FF);

	if (!flag)
	{
		sendError(BUFFER_OVERFLOW, REPLY_GET_STATUS);
		return;
	}

	sendData();
}

//������������� ����
//�������� ��������� �������� UID
void initChip()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_INIT_CHIP);
		return;
	}

	digitalWrite(LED_PIN, HIGH);
	SPI.begin();      // Init SPI bus
	mfrc522.PCD_Init();   // Init MFRC522
	mfrc522.PCD_SetAntennaGain(gainCoeff);

	// Look for new cards
	if (!mfrc522.PICC_IsNewCardPresent())
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(NO_CHIP, REPLY_INIT_CHIP);
		return;
	}
	// Select one of the cards
	if (!mfrc522.PICC_ReadCardSerial())
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(NO_CHIP, REPLY_INIT_CHIP);
		return;
	}

	//������������� ��������� ������ ���� ����� ������������� ���������� ��� �� ���� ��������� ������ �� �������� �������
	if (!ntagRead4pages(PAGE_INIT_TIME))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(READ_ERROR, REPLY_INIT_CHIP);
		return;
	}
	uint32_t initTime = ntag_page[0];
	initTime <<= 8;
	initTime += ntag_page[1];
	initTime <<= 8;
	initTime += ntag_page[2];
	initTime <<= 8;
	initTime += ntag_page[3];
	DS3231_get(&systemTime);
	if ((systemTime.unixtime - initTime) < maxTimeInit)
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(LOW_INIT_TIME, REPLY_INIT_CHIP);
		return;
	}

	//0-1: ����� �������
	//2-3 : ����� ����������
	//����� ���!!! 4 - 11: UID ����

	//��������� UID �����
	/*if (!ntagRead4pages(PAGE_UID))
	{
		SPI.end();
	digitalWrite(LED_PIN, LOW);
		sendError(READ_ERROR, REPLY_INIT_CHIP);
		return;
	}
	bool flag = true;
	for (uint8_t i = 0; i <= 7; i++)
	{
		if (ntag_page[i] != uartBuffer[DATA_START_BYTE + 4 + i])
		{
			flag = false;
			break;
		}
	}
	if (!flag)
	{
		SPI.end();
	digitalWrite(LED_PIN, LOW);
		sendError(WRONG_UID, REPLY_INIT_CHIP);
		return;
	}*/

	//��������� ����� 0xFF
	uint8_t dataBlock[4] = { 255,255,255,255 };
	/*for (uint8_t page = PAGE_CHIP_NUM; page < NTAG215_MAX_PAGE; page++)
	{
		if (!ntagWritePage(dataBlock, page))
		{
			SPI.end();
	digitalWrite(LED_PIN, LOW);
			sendError(WRITE_ERROR, REPLY_INIT_CHIP);
			return;
		}
	}*/

	//��������� ����� 0x00
	for (uint8_t i = 0; i < 4; i++) dataBlock[i] = 0;
	for (uint8_t page = PAGE_CHIP_NUM; page < TAG_MAX_PAGE; page++)
	{
		if (!ntagWritePage(dataBlock, page))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(WRITE_ERROR, REPLY_INIT_CHIP);
			return;
		}
	}

	//����� ������ �� �����
	//����� �������, ��� ����, ������ �������� �������	
	dataBlock[0] = uartBuffer[DATA_START_BYTE];
	dataBlock[1] = uartBuffer[DATA_START_BYTE + 1];
	dataBlock[2] = NTAG_MARK;
	dataBlock[3] = FW_VERSION;
	if (!ntagWritePage(dataBlock, PAGE_CHIP_NUM))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(WRITE_ERROR, REPLY_INIT_CHIP);
		return;
	}

	//����� �� ����� ������� �����
	uint32_t tmpTime = systemTime.unixtime;
	dataBlock[0] = (tmpTime & 0xFF000000) >> 24;
	dataBlock[1] = (tmpTime & 0x00FF0000) >> 16;
	dataBlock[2] = (tmpTime & 0x0000FF00) >> 8;
	dataBlock[3] = tmpTime & 0x000000FF;
	if (!ntagWritePage(dataBlock, PAGE_INIT_TIME))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(WRITE_ERROR, REPLY_INIT_CHIP);
		return;
	}

	//����� ����������
	dataBlock[0] = uartBuffer[DATA_START_BYTE + 2];
	dataBlock[1] = uartBuffer[DATA_START_BYTE + 3];
	dataBlock[2] = 0;
	dataBlock[3] = 0;
	if (!ntagWritePage(dataBlock, PAGE_TEAM_MASK))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(WRITE_ERROR, REPLY_INIT_CHIP);
		return;
	}

	//�������� UID �����
	if (!ntagRead4pages(PAGE_UID))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(READ_ERROR, REPLY_INIT_CHIP);
		return;
	}
	SPI.end();
	digitalWrite(LED_PIN, LOW);

	init_package(REPLY_INIT_CHIP);
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_INIT_CHIP);
		return;
	}

	//��������� � ����� ����� �������������
	bool flag = true;
	flag &= addData((tmpTime & 0xFF000000) >> 24);
	flag &= addData((tmpTime & 0x00FF0000) >> 16);
	flag &= addData((tmpTime & 0x0000FF00) >> 8);
	flag &= addData(tmpTime & 0x000000FF);
	if (!flag)
	{
		sendError(BUFFER_OVERFLOW, REPLY_INIT_CHIP);
		return;
	}

	//��������� � ����� UID
	for (uint8_t i = 0; i <= 7; i++)
	{
		if (!addData(ntag_page[i]))
		{
			sendError(BUFFER_OVERFLOW, REPLY_INIT_CHIP);
			return;
		}
	}
	sendData();
}

//�������� ��������� ������������ �������
void getLastTeams()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_GET_LAST_TEAMS);
		return;
	}

	init_package(REPLY_GET_LAST_TEAMS);

	bool flag = true;
	//0: ��� ������
	flag &= addData(OK);

	//������ ��������� ������
	for (uint8_t i = 0; i < lastTeamsLength * 2; i++)
	{
		flag &= addData(lastTeams[i]);
	}
	if (!flag)
	{
		sendError(BUFFER_OVERFLOW, REPLY_GET_LAST_TEAMS);
		return;
	}

	sendData();
}

//�������� ������ � ������� �� �����
void getTeamRecord()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_GET_TEAM_RECORD);
		return;
	}

	//0-1: ����� ������
	uint16_t recordNumber = uartBuffer[DATA_START_BYTE];
	recordNumber <<= 8;
	recordNumber += uartBuffer[DATA_START_BYTE + 1];

	if (recordNumber < 1 || recordNumber >= LOG_LENGTH)
	{
		sendError(WRONG_TEAM, REPLY_GET_TEAM_RECORD);
		return;
	}

	init_package(REPLY_GET_TEAM_RECORD);
	//0: ��� ������                            
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_GET_TEAM_RECORD);
		return;
	}

	//���� ������ ���� �����
	if (!readTeamFromFlash(recordNumber))
	{
		sendError(NO_DATA, REPLY_GET_TEAM_RECORD);
		return;
	}

	//1-2: ����� �������
	//3-6 : ����� �������������
	//7-8: ����� �������	
	//9-12 : ����� ��������� ������� �� �������
	//13: ������� ����������� �������
	//13-14: ������� ����������� ������� for 2 byte version
	bool flag = true;
	//for (uint8_t i = 0; i < 14; i++)
	for (uint8_t i = 0; i < 13; i++)
	{
		if (!addData(ntag_page[i]))
		{
			sendError(BUFFER_OVERFLOW, REPLY_GET_LAST_TEAMS);
			return;
		}
	}

	sendData();
}

//������ �������� � �����.
void readCardPages()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_READ_CARD_PAGE);
		return;
	}

	digitalWrite(LED_PIN, HIGH);
	SPI.begin();      // Init SPI bus
	mfrc522.PCD_Init();   // Init MFRC522
	mfrc522.PCD_SetAntennaGain(gainCoeff);

	// Look for new cards
	if (!mfrc522.PICC_IsNewCardPresent())
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(NO_CHIP, REPLY_READ_CARD_PAGE);
		return;
	}
	// Select one of the cards
	if (!mfrc522.PICC_ReadCardSerial())
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(NO_CHIP, REPLY_READ_CARD_PAGE);
		return;
	}

	uint8_t pageFrom = uartBuffer[DATA_START_BYTE];
	uint8_t pageTo = uartBuffer[DATA_START_BYTE + 1];

	init_package(REPLY_READ_CARD_PAGE);

	//0: ��� ������
	//1-8: UID ����
	//9-12: ������ �� �������� �����(4 �����)
	if (!addData(OK))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(BUFFER_OVERFLOW, REPLY_READ_CARD_PAGE);
		return;
	}

	if (!ntagRead4pages(PAGE_UID))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(READ_ERROR, REPLY_READ_CARD_PAGE);
		return;
	}
	for (uint8_t i = 0; i <= 7; i++)
	{
		if (!addData(ntag_page[i]))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(BUFFER_OVERFLOW, REPLY_READ_CARD_PAGE);
			return;
		}
	}

	while (pageFrom <= pageTo)
	{
		//0: ����� �������� �����
		if (!ntagRead4pages(pageFrom))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(READ_ERROR, REPLY_READ_CARD_PAGE);
			return;
		}
		uint8_t n = (pageTo - pageFrom + 1);
		if (n > 4) n = 4;
		for (uint8_t i = 0; i < n; i++)
		{
			if (!addData(pageFrom))
			{
				SPI.end();
				digitalWrite(LED_PIN, LOW);
				sendError(BUFFER_OVERFLOW, REPLY_READ_CARD_PAGE);
				return;
			}
			for (uint8_t j = 0; j < 4; j++)
			{
				if (!addData(ntag_page[i * 4 + j]))
				{
					SPI.end();
					digitalWrite(LED_PIN, LOW);
					sendError(BUFFER_OVERFLOW, REPLY_READ_CARD_PAGE);
					return;
				}
			}
			pageFrom++;
		}
	}
	SPI.end();
	digitalWrite(LED_PIN, LOW);

	sendData();
}

//�������� ����� ������� � ������
void updateTeamMask()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_UPDATE_TEAM_MASK);
		return;
	}

	//0-1: ����� �������
	//2-5: ����� ������ ����
	//6-7: ����� ����������

	if (stationMode != 0)
	{
		for (uint8_t i = 0; i < 8; i++)
		{
			newTeamMask[i] = uartBuffer[DATA_START_BYTE + i];
		}
	}
	else
	{
		digitalWrite(LED_PIN, HIGH);
		SPI.begin();      // Init SPI bus
		mfrc522.PCD_Init();   // Init MFRC522
		mfrc522.PCD_SetAntennaGain(gainCoeff);

		// Look for new cards
		if (!mfrc522.PICC_IsNewCardPresent())
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(NO_CHIP, REPLY_UPDATE_TEAM_MASK);
			return;
		}

		// Select one of the cards
		if (!mfrc522.PICC_ReadCardSerial())
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(NO_CHIP, REPLY_UPDATE_TEAM_MASK);
			return;
		}

		//��������� ����� �������
		if (!ntagRead4pages(PAGE_CHIP_NUM))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(READ_ERROR, REPLY_UPDATE_TEAM_MASK);
			return;
		}
		if (ntag_page[0] != uartBuffer[DATA_START_BYTE] || ntag_page[1] != uartBuffer[DATA_START_BYTE + 1])
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(WRONG_TEAM, REPLY_UPDATE_TEAM_MASK);
			return;
		}

		//��������� ����� ������ ����
		if (!ntagRead4pages(PAGE_INIT_TIME))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(READ_ERROR, REPLY_UPDATE_TEAM_MASK);
			return;
		}

		if (ntag_page[0] != uartBuffer[DATA_START_BYTE + 2]
			|| ntag_page[1] != uartBuffer[DATA_START_BYTE + 3]
			|| ntag_page[2] != uartBuffer[DATA_START_BYTE + 4]
			|| ntag_page[3] != uartBuffer[DATA_START_BYTE + 5])
		{
#ifdef DEBUG
			/*DebugSerial.println(F("chip init time wrong"));
			DebugSerial.print(String(ntag_page[0], HEX));
			DebugSerial.print(F("="));
			DebugSerial.println(String(uartBuffer[DATA_START_BYTE + 2], HEX));
			DebugSerial.print(F("upd mask chip error"));
			DebugSerial.print(String(ntag_page[1], HEX));
			DebugSerial.print(F("="));
			DebugSerial.println(String(uartBuffer[DATA_START_BYTE + 3], HEX));
			DebugSerial.print(F("upd mask chip error"));
			DebugSerial.print(String(ntag_page[2], HEX));
			DebugSerial.print(F("="));
			DebugSerial.println(String(uartBuffer[DATA_START_BYTE + 4], HEX));
			DebugSerial.print(F("upd mask chip error"));
			DebugSerial.print(String(ntag_page[3], HEX));
			DebugSerial.print(F("="));
			DebugSerial.println(String(uartBuffer[DATA_START_BYTE + 5], HEX));*/
#endif
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(WRONG_CHIP, REPLY_UPDATE_TEAM_MASK);
			return;
		}

		//�������� �������� �� �����
		uint8_t dataBlock[4] = { uartBuffer[DATA_START_BYTE + 6],uartBuffer[DATA_START_BYTE + 7], ntag_page[6],ntag_page[7] };
		if (!ntagWritePage(dataBlock, PAGE_TEAM_MASK))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			sendError(WRITE_ERROR, REPLY_UPDATE_TEAM_MASK);
			return;
		}
		SPI.end();
		digitalWrite(LED_PIN, LOW);
	}

	init_package(REPLY_UPDATE_TEAM_MASK);
	//0: ��� ������
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_UPDATE_TEAM_MASK);
		return;
	}
	sendData();
}

//����� ���������� � �� 4 ����� � ��������� ��������
void writeCardPage()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_WRITE_CARD_PAGE);
		return;
	}

	digitalWrite(LED_PIN, HIGH);
	SPI.begin();      // Init SPI bus
	mfrc522.PCD_Init();   // Init MFRC522
	mfrc522.PCD_SetAntennaGain(gainCoeff);

	// Look for new cards
	if (!mfrc522.PICC_IsNewCardPresent())
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(NO_CHIP, REPLY_WRITE_CARD_PAGE);
		return;
	}

	// Select one of the cards
	if (!mfrc522.PICC_ReadCardSerial())
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(NO_CHIP, REPLY_WRITE_CARD_PAGE);
		return;
	}

	//0-7: UID ����
	//8: ����� ��������
	//9-12: ������ ��� ������ (4 �����)

	//��������� UID
	if (!ntagRead4pages(PAGE_UID))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(READ_ERROR, REPLY_WRITE_CARD_PAGE);
		return;
	}
	bool flag = false;
	for (uint8_t i = 0; i <= 7; i++)
	{
		if (uartBuffer[DATA_START_BYTE + i] != 0xff && ntag_page[i] != uartBuffer[DATA_START_BYTE + i])
		{
			flag = true;
			break;
		}
	}
	if (flag)
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(WRONG_UID, REPLY_WRITE_CARD_PAGE);
		return;
	}

	//�������� ��������
	uint8_t dataBlock[] =
	{
		uartBuffer[DATA_START_BYTE + 9],
		uartBuffer[DATA_START_BYTE + 10],
		uartBuffer[DATA_START_BYTE + 11],
		uartBuffer[DATA_START_BYTE + 12]
	};
	if (!ntagWritePage(dataBlock, uartBuffer[DATA_START_BYTE + 8]))
	{
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		sendError(WRITE_ERROR, REPLY_WRITE_CARD_PAGE);
		return;
	}
	SPI.end();
	digitalWrite(LED_PIN, LOW);

	init_package(REPLY_WRITE_CARD_PAGE);

	//0: ��� ������
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_WRITE_CARD_PAGE);
		return;
	}
	sendData();
}

//������ ����
void readFlash()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_READ_FLASH);
		return;
	}

	//0-3: ����� ������ ������
	//4-7: ����� ����� ������
	uint32_t startAddress = uartBuffer[DATA_START_BYTE];
	startAddress <<= 8;
	startAddress += uartBuffer[DATA_START_BYTE + 1];
	startAddress <<= 8;
	startAddress += uartBuffer[DATA_START_BYTE + 2];
	startAddress <<= 8;
	startAddress += uartBuffer[DATA_START_BYTE + 3];

	uint32_t endAddress = uartBuffer[DATA_START_BYTE + 4];
	endAddress <<= 8;
	endAddress += uartBuffer[DATA_START_BYTE + 5];
	endAddress <<= 8;
	endAddress += uartBuffer[DATA_START_BYTE + 6];
	endAddress <<= 8;
	endAddress += uartBuffer[DATA_START_BYTE + 7];

#ifdef DEBUG
	/*DebugSerial.print(F("flash read="));
	DebugSerial.print(String(startAddress));
	DebugSerial.print(F("-"));
	DebugSerial.println(String(endAddress));*/
#endif

	init_package(REPLY_READ_FLASH);
	//0: ��� ������
	//0-3: ����� ������ ������
	//4-n: ������ �� �����
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_READ_FLASH);
		return;
	}

	bool flag = true;
	flag &= addData((startAddress & 0xFF000000) >> 24);
	flag &= addData((startAddress & 0x00FF0000) >> 16);
	flag &= addData((startAddress & 0x0000FF00) >> 8);
	flag &= addData(startAddress & 0x000000FF);

	for (; startAddress <= endAddress; startAddress++)
	{
		uint8_t b = SPIflash.readByte(startAddress);
		flag &= addData(b);
#ifdef DEBUG
		/*DebugSerial.print(String(i));
		DebugSerial.print("=");
		if (b < 0x10) DebugSerial.print(F("0"));
		DebugSerial.println(String(b, HEX));*/
#endif
	}
	if (!flag)
	{
		sendError(BUFFER_OVERFLOW, REPLY_READ_FLASH);
		return;
	}

	sendData();
}

//����� � ����
//!!! ������� ������ ������ ����� ��������
bool writeFlash()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_WRITE_FLASH);
		return;
	}

	//0-3: ����� ������ ������
	//1: ���-�� ���������� ���� (��� ��������)
	uint32_t startAddress = uartBuffer[DATA_START_BYTE];
	startAddress <<= 8;
	startAddress += uartBuffer[DATA_START_BYTE + 1];
	startAddress <<= 8;
	startAddress += uartBuffer[DATA_START_BYTE + 2];
	startAddress <<= 8;
	startAddress += uartBuffer[DATA_START_BYTE + 3];

	init_package(REPLY_WRITE_FLASH);

	uint32_t i = startAddress + uartBuffer[LENGTH_BYTE] - 4;
	uint8_t n = 0;
	while (startAddress < i)
	{
		if (!SPIflash.writeByte(startAddress, uartBuffer[DATA_START_BYTE + 4 + n]))
		{
			sendError(WRITE_ERROR, REPLY_WRITE_FLASH);
			return false;
		}
		startAddress++;
		n++;
	}

	//0: ��� ������
	//1: ���-�� ���������� ���� (��� ��������)
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_WRITE_FLASH);
		return false;
	}
	if (!addData(n))
	{
		sendError(BUFFER_OVERFLOW, REPLY_WRITE_FLASH);
		return false;
	}

	sendData();
}

//������� ������ ����� (4096 ����)
//�����������, ������ ������� ������ 0-� ������.
void eraseFlashSector()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_ERASE_FLASH_SECTOR);
		return;
	}

	uint32_t sectordNumber = uartBuffer[DATA_START_BYTE];
	sectordNumber <<= 8;
	sectordNumber += uartBuffer[DATA_START_BYTE + 1];
#ifdef DEBUG
	DebugSerial.print(F("erasing "));
	DebugSerial.println(String(sectordNumber));
#endif

	if (!SPIflash.eraseSector(sectordNumber))
	{
		sendError(ERASE_ERROR, REPLY_ERASE_FLASH_SECTOR);
		return;
	}

	init_package(REPLY_ERASE_FLASH_SECTOR);
	//0: ��� ������
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_ERASE_FLASH_SECTOR);
		return;
	}
	sendData();
}

//������ ������������ �������: ����� �������, ����� ������, �����. ��������� ����������, ������ �����
//���������� �����. �������� RFID
void getConfig()
{
	//0: ��� ������
	//1: ������ ��������
	//2: ����� ������
	//3: ��� ����� (������� ������, � ���������� �� ���������� ����� ������ �� ������� ������ "�������" �������)
	//4-7: ������� ���� - ������
	//8-11: ������ ������� ���� - ������
	//12-15: ����������� ��������� ����������(float, 4 bytes) - ������ ��������� ����������� �� ���������� � ������� ����� � ����� �����������
	init_package(REPLY_GET_CONFIG);

	bool flag = true;
	flag &= addData(OK);
	flag &= addData(FW_VERSION);
	flag &= addData(stationMode);
	flag &= addData(NTAG_MARK);

	uint32_t n = SPIflash.getCapacity();
	flag &= addData((n & 0xFF000000) >> 24);
	flag &= addData((n & 0x00FF0000) >> 16);
	flag &= addData((n & 0x0000FF00) >> 8);
	flag &= addData(n & 0x000000FF);

	n = SPIflash.getMaxPage();
	flag &= addData((n & 0xFF000000) >> 24);
	flag &= addData((n & 0x00FF0000) >> 16);
	flag &= addData((n & 0x0000FF00) >> 8);
	flag &= addData(n & 0x000000FF);

	byte v[4];
	floatToByte(v, voltageCoeff);
	flag &= addData(v[0]);
	flag &= addData(v[1]);
	flag &= addData(v[2]);
	flag &= addData(v[3]);

	flag &= addData(gainCoeff);

	if (!flag)
	{
		sendError(BUFFER_OVERFLOW, REPLY_GET_CONFIG);
		return;
	}

	sendData();
}

//��������� �����. ��������� ADC � ���������� ��� ������������ �������� 10��� + 2.2���
void setVCoeff()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_SET_KOEFF);
		return;
	}

	//0-3: �����.
	union Convert
	{
		float number;
		byte byte[4];
	} p;

	for (byte i = 0; i < 4; i++)
	{
		p.byte[i] = uartBuffer[DATA_START_BYTE + i];
		if (!eepromwrite(EEPROM_VOLTAGE_KOEFF + i * 3, uartBuffer[DATA_START_BYTE + i])) errorBeep(1); //Read the station number from the EEPROM
	}
	voltageCoeff = p.number;

	init_package(REPLY_SET_KOEFF);
	//0: ��� ������
	//1...: ������ �� �����
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_SET_KOEFF);
		return;
	}

	sendData();
}

//��������� �����. �������� ��� RFID
void setGain()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_SET_GAIN);
		return;
	}

	//0: �����.
	gainCoeff = uartBuffer[DATA_START_BYTE] & 0x70;
	if (!eepromwrite(EEPROM_GAIN, gainCoeff)) errorBeep(1); //Read the station number from the EEPROM

	init_package(REPLY_SET_GAIN);
	//0: ��� ������
	if (!addData(OK))
	{
		sendError(BUFFER_OVERFLOW, REPLY_SET_GAIN);
		return;
	}

	sendData();
}

//��������� �����. �������� ��� RFID
void setChipType()
{
	//���� ����� ������� �� ��������� � ���������� � ������, �� �����
	if (stationNumber != uartBuffer[STATION_NUMBER_BYTE])
	{
		sendError(WRONG_STATION, REPLY_SET_CHIP_TYPE);
		return;
	}

	//0: ��� ����
	chipType = uartBuffer[DATA_START_BYTE];
	bool e = selectChipType(chipType);
	if (e)
	{
		if (!eepromwrite(EEPROM_CHIP_TYPE, chipType)) errorBeep(1); //Read the station number from the EEPROM
	}

	init_package(REPLY_SET_CHIP_TYPE);
	//0: ��� ������
	byte error = OK;
	if (e) error = WRONG_CHIP_TYPE;
	if (!addData(error))
	{
		sendError(BUFFER_OVERFLOW, REPLY_SET_CHIP_TYPE);
		return;
	}

	sendData();
}

//��������� ������������ ����
void processRfidCard()
{
	if (stationNumber == 0 || stationNumber == 0xff) return;
	DS3231_get(&systemTime);
	uint32_t checkTime = systemTime.unixtime;

	//�������� SPI ���� ����� ������. ���� �� ������� ������� �� ������� ������ �����
	SPI.begin();      // Init SPI bus
	mfrc522.PCD_Init();    // Init MFRC522
	mfrc522.PCD_SetAntennaGain(gainCoeff);

	// Look for new cards
	if (!mfrc522.PICC_IsNewCardPresent())
	{
		SPI.end();
		return;
	}

	// Select one of the cards
	if (!mfrc522.PICC_ReadCardSerial())
	{
		SPI.end();
		return;
	}

	//������ ���� ����������
	if (!ntagRead4pages(PAGE_CHIP_NUM))
	{
		SPI.end();
		//errorBeep(2);
		return;
	}

	/*
	���������
	1 - ���� � �������� �0 ��� > LOG_LENGTH
	2 - ���, ������� ��������� � ��� ���������� (� lastTeams[])
	3 - ��� ����� ��������� �������� �������������
	*/

	//�� ������� �� ������ ���? ��������� �������� � �����
	uint32_t timeInit = ntag_page[4];
	timeInit = timeInit << 8;
	timeInit += ntag_page[5];
	timeInit = timeInit << 8;
	timeInit += ntag_page[6];
	timeInit = timeInit << 8;
	timeInit += ntag_page[7];
	if ((systemTime.unixtime - timeInit) > maxTimeInit)
	{
		SPI.end();
		//errorBeep(3);
		return;
	}

	//�� ����� �� ����� ���� 0 ��� >= LOG_LENGTH
	uint16_t chipNum = (ntag_page[0] << 8) + ntag_page[1];
	if (chipNum < 1 || chipNum >= LOG_LENGTH)
	{
		SPI.end();
		//errorBeep(4);
		return;
	}

	//�� ���� �� �������� � ���� �����?
	//0-1: ����� �������
	//2-5: ����� ������ ����
	//6-7: ����� ����������
	if (ntag_page[0] == newTeamMask[0]
		&& ntag_page[1] == newTeamMask[1]
		&& ntag_page[4] == newTeamMask[2]
		&& ntag_page[5] == newTeamMask[3]
		&& ntag_page[6] == newTeamMask[4]
		&& ntag_page[7] == newTeamMask[5])
	{
		if ((ntag_page[8] != newTeamMask[6] || ntag_page[9] != newTeamMask[7]))
		{
			digitalWrite(LED_PIN, HIGH);
			uint8_t dataBlock[4] = { newTeamMask[6], newTeamMask[7], ntag_page[10], ntag_page[11] };
			if (!ntagWritePage(dataBlock, PAGE_TEAM_MASK))
			{
				//sendError(WRITE_ERROR, REPLY_UPDATE_TEAM_MASK);
				SPI.end();
				digitalWrite(LED_PIN, LOW);
				errorBeep(5);
				return;
			}
		}
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		return;
	}

	//�������� � ������� ��������� ������
	bool flag = false;
	for (uint8_t i = 0; i < lastTeamsLength * 2; i = i + 2)
	{
		if (lastTeams[i] == ntag_page[0] && lastTeams[i + 1] == ntag_page[1])
		{
			flag = true;
			break;
		}
	}

	//���� �� ��� �� �����
	if (!flag && SPIflash.readByte((uint32_t)((uint32_t)chipNum * (uint32_t)LOG_RECORD_LENGTH)) != 255) flag = true;

	//���� ����� ��� ��� �������� ��
	if (!flag || stationMode == MODE_FINISH_KP)
	{
		digitalWrite(LED_PIN, HIGH);
		//���� ��������� �������� �� �����
		uint8_t newPage = findNewPage();

		//������ ������ ��� ������ ���������... ��������, ����������???
		if (newPage < PAGE_DATA_START || newPage >= TAG_MAX_PAGE)
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			errorBeepMs(1000, 2);
			return;
		}

		//����� �� ����� �������
		if (!writeCheckPointToCard(newPage, checkTime))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			errorBeepMs(1000, 3);
			return;
		}

		//��������� � ����� ��������� ������
		addLastTeam(chipNum);
		lastTimeChecked = checkTime;
		if (!flag) totalChipsChecked++;

		//����� ���� ����� �� ����
		if (!writeDumpToFlash(chipNum, checkTime))
		{
			SPI.end();
			digitalWrite(LED_PIN, LOW);
			errorBeepMs(1000, 4);
			return;
		}
		SPI.end();
		digitalWrite(LED_PIN, LOW);
		beep(200, 1);
#ifdef DEBUG
		DebugSerial.print(F("record# "));
		DebugSerial.println(String(chipNum));
#endif
	}
}

bool readUart()
{
	while (Serial.available())
	{
		int c = Serial.read();
		if (c == -1) // can't read stream
		{
#ifdef DEBUG
			//DebugSerial.println(F("read error"));
#endif
			uartBufferPosition = 0;
			receivingData = false;
			errorBeepMs(50, 1);
			return false;
		}
		//0 byte = FE
		else if (uartBufferPosition == 0 && c == 0xfe)
		{
#ifdef DEBUG
			/*DebugSerial.print(F("byte0="));
			if (c < 0x10) DebugSerial.print(F("0"));
			DebugSerial.println(String(byte(c), HEX));*/
#endif
			receivingData = true;
			uartBuffer[uartBufferPosition] = (byte)c;
			uartBufferPosition++;
			// refresh timeout
			receiveStartTime = millis();
		}
		//1st byte = FE
		else if (uartBufferPosition == 1 && c == 0xfe)
		{
#ifdef DEBUG
			/*DebugSerial.print(F("byte1"));
			if (c < 0x10) DebugSerial.print(F("0"));
			DebugSerial.println(String(byte(c), HEX));*/
#endif
			uartBuffer[uartBufferPosition] = (byte)c;
			uartBufferPosition++;
		}
		//2nd byte = FE
		else if (uartBufferPosition == 2 && c == 0xfe)
		{
#ifdef DEBUG
			/*DebugSerial.print(F("byte2"));
			if (c < 0x10) DebugSerial.print(F("0"));
			DebugSerial.println(String(byte(c), HEX));*/
#endif
			uartBuffer[uartBufferPosition] = (byte)c;
			uartBufferPosition++;
		}
		//4th byte = command, length and data
		else if (uartBufferPosition >= STATION_NUMBER_BYTE)
		{
			uartBuffer[uartBufferPosition] = (byte)c;
#ifdef DEBUG
			/*DebugSerial.print(F("byte"));
			DebugSerial.print(String(uartBufferPosition));
			DebugSerial.print(F("="));
			if (c < 0x10) DebugSerial.print(F("0"));
			DebugSerial.println(String(byte(c), HEX));*/
#endif
			//incorrect length
			if (uartBufferPosition == LENGTH_BYTE && uartBuffer[LENGTH_BYTE] > (254 - DATA_START_BYTE))
			{
#ifdef DEBUG
				DebugSerial.println(F("incorrect length"));
#endif
				uartBufferPosition = 0;
				receivingData = false;
				beep(50, 2);
				return false;
			}

			//packet is received
			if (uartBufferPosition > LENGTH_BYTE && uartBufferPosition >= DATA_START_BYTE + uartBuffer[LENGTH_BYTE])
			{
				//crc matching
#ifdef DEBUG
				/*DebugSerial.print(F("received packet expected CRC="));
				DebugSerial.println(String(crcCalc(uartBuffer, STATION_NUMBER_BYTE, uartBufferPosition - 1), HEX));*/
#endif
				if (uartBuffer[uartBufferPosition] == crcCalc(uartBuffer, STATION_NUMBER_BYTE, uartBufferPosition - 1))
				{
#ifdef DEBUG
					/*DebugSerial.print(F("Command received:"));
					for (uint8_t i = 0; i <= uartBufferPosition; i++)
					{
						DebugSerial.print(F(" "));
						if (uartBuffer[i] < 0x10) DebugSerial.print(F("0"));
						DebugSerial.print(String(uartBuffer[i], HEX));
			}
					DebugSerial.println();*/
#endif
					uartBufferPosition = 0;
					receivingData = false;
					return true;
				}
				else // CRC not correct
				{
#ifdef DEBUG
					DebugSerial.println(F("incorrect crc"));
#endif
					uartBufferPosition = 0;
					receivingData = false;
					beep(50, 2);
					return false;
				}
			}
			uartBufferPosition++;
		}
		else
		{
#ifdef DEBUG
			DebugSerial.println(F("unexpected byte"));
#endif
			receivingData = false;
			uartBufferPosition = 0;
			beep(50, 3);
		}
	}
	return false;
}

// Internal functions

//������ ������ �������
uint16_t getBatteryLevel()
{
	const uint8_t MeasurementsToAverage = 16;
	uint16_t val = 0;
	uint32_t AverageValue = analogRead(BATTERY_PIN);
	for (uint8_t i = 1; i < MeasurementsToAverage; ++i)
	{
		val = analogRead(BATTERY_PIN);
		AverageValue = (AverageValue + val) / 2;
		//delay(1);
	}
	return AverageValue;
}

//������ � ������ � ������������� ���������������
bool eepromwrite(uint16_t adr, uint8_t val)
{
	for (uint8_t i = 0; i < 3; i++)
	{
		EEPROM.write(adr + i, val);
		if (EEPROM.read(adr + i) != val) return false;
	}
	return true;
}

//���������� �� ������ � ������ �������������� ��������������
int eepromread(uint16_t adr)
{
	uint8_t byte1 = EEPROM.read(adr);
	uint8_t byte2 = EEPROM.read(adr + 1);
	uint8_t byte3 = EEPROM.read(adr + 2);

	// ���������� ��� ���������� ��� �� ����
	if (byte1 == byte2 && byte1 == byte3)
	{
		return byte1;
	}
	if (byte1 == byte2)
	{
		return byte1;
	}
	if (byte1 == byte3)
	{
		return byte1;
	}
	if (byte2 == byte3)
	{
		return byte2;
	}
	return -1;
}

//������ �������, ������������ ������� � �������� � �� � ����� ����������
void beep(uint16_t ms, uint8_t n)
{
	for (uint8_t i = 0; i < n; i++)
	{
		digitalWrite(LED_PIN, HIGH);
		tone(BUZZER_PIN, 4000, ms);
		delay(ms);
		digitalWrite(LED_PIN, LOW);
		if ((n - i) != 0)
		{
			delay(ms);
		}
	}
}

//������ �������, ������������ ������� � �������� � �� � ����� ����������
void errorBeepMs(uint16_t ms, uint8_t n)
{
	for (uint8_t i = 0; i < n; i++)
	{
		digitalWrite(ERROR_LED_PIN, HIGH);
		tone(BUZZER_PIN, 500, ms);
		digitalWrite(ERROR_LED_PIN, LOW);
		if ((n - i) != 0)
		{
			delay(ms);
		}
	}
}

void errorBeep(uint8_t n)
{
	uint16_t ms = 200;
	for (uint8_t i = 0; i < n; i++)
	{
		digitalWrite(ERROR_LED_PIN, HIGH);
		//tone(BUZZER_PIN, 500, ms);
		tone(BUZZER_PIN, 500, ms);
		digitalWrite(ERROR_LED_PIN, LOW);
		if ((n - i) != 0)
		{
			delay(ms);
		}
	}
}

//������������� ������ ������
void init_package(uint8_t command)
{
	uartBuffer[0] = uartBuffer[1] = uartBuffer[2] = 0xFE;
	uartBuffer[3] = stationNumber;
	uartBuffer[COMMAND_BYTE] = command;
	uartBufferPosition = DATA_START_BYTE;
}

//���������� ������ � �����
//��������� ������������ ������������
bool addData(uint8_t data)
{
	if (uartBufferPosition > 254)
	{
		//sendError(BUFFER_OVERFLOW, uartBuffer[COMMAND_BYTE]);
		return false;
	}
	uartBuffer[uartBufferPosition] = data;
	uartBufferPosition++;
	return true;
}

//�������� ������ ������.
void sendData()
{
	uartBuffer[LENGTH_BYTE] = uartBufferPosition - COMMAND_BYTE - 1;
	uartBuffer[uartBufferPosition] = crcCalc(uartBuffer, STATION_NUMBER_BYTE, uartBufferPosition - 1);
#ifdef DEBUG
	/*DebugSerial.print(F("Sending:"));
	for (uint8_t i = 0; i < uartBufferPosition; i++)
	{
		DebugSerial.print(F(" "));
		if (uartBuffer[i] < 0x10) DebugSerial.print(F("0"));
		DebugSerial.print(String(uartBuffer[i], HEX));
	}
	DebugSerial.println();*/
#endif
	Serial.write(uartBuffer, uartBufferPosition + 1);
	uartBufferPosition = 0;
}

//������ �������� (4 �����) � ���
bool ntagWritePage(uint8_t * dataBlock, uint8_t pageAdr)
{
	const uint8_t sizePageNtag = 4;

	uint8_t n = 0;
	status = MFRC522::STATUS_ERROR;
	while (status != MFRC522::STATUS_OK && n < 3)
	{
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Ultralight_Write(pageAdr, dataBlock, sizePageNtag);
		n++;
		if (!status) delay(10);
	}

	if (status != MFRC522::STATUS_OK)
	{
		return false;
	}

	uint8_t buffer[18];
	uint8_t size = sizeof(buffer);


	n = 0;
	status = MFRC522::STATUS_ERROR;
	while (status != MFRC522::STATUS_OK && n < 3)
	{
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(pageAdr, buffer, &size);
		n++;
	}
	if (status != MFRC522::STATUS_OK)
	{
		return false;
	}

	for (uint8_t i = 0; i < 4; i++)
	{
		if (buffer[i] != dataBlock[i])
		{
			return false;
		}
	}

	return true;
}

//������ 4-� ������� (16 ����) �� ����
bool ntagRead4pages(uint8_t pageAdr)
{
	uint8_t size = 18;
	uint8_t buffer[18];

	uint8_t n = 0;
	status = MFRC522::STATUS_ERROR;
	while (status != MFRC522::STATUS_OK && n < 3)
	{
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(pageAdr, buffer, &size);
		n++;
	}

	if (status != MFRC522::STATUS_OK)
	{
		return false;
	}

	for (uint8_t i = 0; i < 16; i++)
	{
		ntag_page[i] = buffer[i];
	}
	return true;
}

//����� �� ����� ����� � ������� �������
bool writeCheckPointToCard(uint8_t newPage, uint32_t checkTime)
{
	uint8_t dataBlock[4];
	dataBlock[0] = stationNumber;
	dataBlock[1] = (checkTime & 0x00FF0000) >> 16;
	dataBlock[2] = (checkTime & 0x0000FF00) >> 8;
	dataBlock[3] = (checkTime & 0x000000FF);

	if (!ntagWritePage(dataBlock, newPage))
	{
		return false;
	}
	return true;
}

//����� ��������� ���������� �������� �� ��������.
//����������� � ��������� ����
uint8_t findNewPage()
{
	/*uint8_t page = TAG_MAX_PAGE - 4;
	while (page >= PAGE_DATA_START)
	{
		if (!ntagRead4pages(page))
		{
			return 0;
		}
		for (int n = 3; n >= 0; n--)
		{
			if (ntag_page[0 + n * 4] != 0)
			{
				if (stationMode == MODE_FINISH_KP && ntag_page[0 + n * 4] == stationNumber)
				{
					return page;
				}
				else
				{
					return ++page;
				}
			}
			page--;
		}
	}
	return TAG_MAX_PAGE;*/

	uint8_t page = PAGE_DATA_START;
	while (page < TAG_MAX_PAGE)
	{
		if (!ntagRead4pages(page))
		{
			return 0;
		}
		for (uint8_t n = 0; n < 4; n++)
		{
			//1) ������� �������� ������
			//2) ����� ������ �������
			if (ntag_page[n * 4] == 0)
			{
				if (!ntagRead4pages(page - 1))
				{
					return 0;
				}
				if (ntag_page[0] == stationNumber) page--;
				return page;
			}
			//2) ����� ������ �������, � ����. �������� ������
			//if (ntag_page[0 + n * 4] == 0 || (ntag_page[0 + n * 4] == stationNumber && ntag_page[0 + 4 + n * 4] == 0)) return page;
			page++;
		}
	}
	return TAG_MAX_PAGE;


	/*uint8_t finishpage = TAG_MAX_PAGE - 1;
	uint8_t startpage = PAGE_DATA_START;
	uint8_t page = (finishpage + startpage) / 2;
	while (1)
	{
		if (finishpage == startpage)
		{
			return (finishpage);
		}
		page = (finishpage + startpage) / 2;
		if (!ntagRead4pages(page))
		{
			return 0;
		}
		if (ntag_page[0] == 0)
		{
			finishpage = (finishpage - startpage) / 2 + startpage;
		}
		else
		{
			startpage = finishpage - (finishpage - startpage) / 2;
		}
	}*/
}

//����� ���� ����� � ���
//!!! ������� ������ ������ ����� ��������. ������� ������� �� 4��
byte writeDumpToFlash(uint16_t recordNum, uint32_t checkTime)
{
	//����� �������� � ��������
	uint32_t teamFlashAddress = (uint32_t)(uint32_t(recordNum) * (uint32_t)LOG_RECORD_LENGTH);
#ifdef DEBUG
	DebugSerial.print(F("Flash address: "));
	DebugSerial.println(String(pageFlash));
#endif

	//���� ����� �������� �������, ��, ��������, ���� ���������� ����������.
	//��������: 1) �� ��������� �������� ��� ����������; 2) ����������� �������/��������� �������� (���� �� 4 �����, � �� �� 16) - ��������, ���������
	if (stationMode == MODE_FINISH_KP && SPIflash.readByte(teamFlashAddress) != 255)
	{
		//SPIflash.eraseSector((long)((float)pageFlash / 4096));
#ifdef DEBUG
		DebugSerial.print(F("erased sector: "));
		DebugSerial.println(String((uint32_t)((uint32_t)pageFlash / (uint32_t)256)));
#endif
	}

	//save basic parameters
	if (!ntagRead4pages(PAGE_CHIP_NUM))
	{
		return false;
	}
	bool flag = true;
	//1-2: ����� �������
	flag &= SPIflash.writeByte(teamFlashAddress, ntag_page[0]);
	flag &= SPIflash.writeByte(teamFlashAddress + 1, ntag_page[1]);
	//3-6: ����� �������������
	flag &= SPIflash.writeByte(teamFlashAddress + 2, ntag_page[4]);
	flag &= SPIflash.writeByte(teamFlashAddress + 3, ntag_page[5]);
	flag &= SPIflash.writeByte(teamFlashAddress + 4, ntag_page[6]);
	flag &= SPIflash.writeByte(teamFlashAddress + 5, ntag_page[7]);
	//7-8: ����� �������
	flag &= SPIflash.writeByte(teamFlashAddress + 6, ntag_page[8]);
	flag &= SPIflash.writeByte(teamFlashAddress + 7, ntag_page[9]);
	//9-12: ����� ��������� ������� �� �������
	flag &= SPIflash.writeByte(teamFlashAddress + 8, (checkTime & 0xFF000000) >> 24);
	flag &= SPIflash.writeByte(teamFlashAddress + 9, (checkTime & 0x00FF0000) >> 16);
	flag &= SPIflash.writeByte(teamFlashAddress + 10, (checkTime & 0x0000FF00) >> 8);
	flag &= SPIflash.writeByte(teamFlashAddress + 11, checkTime & 0x000000FF);
	//if (flag) return false;
#ifdef DEBUG
	DebugSerial.println(F("basics wrote"));
#endif

	//copy card content to flash. ��� �������� �� ������������ � 0
	//teamFlashAddress += 16;
	uint16_t checkCount = 0;
	uint8_t block = 0;
	while (block < TAG_MAX_PAGE - 3)
	{
#ifdef DEBUG
		DebugSerial.print(F("reading page: "));
		DebugSerial.println(String(page));
#endif
		if (!ntagRead4pages(block))
		{
			flag = false;
			break;
			//return false;
		}
		else
		{
			//4 pages in one read block
			for (byte i = 0; i < 4; i++)
			{
				if (block < 8 || ntag_page[i * 4]>0)
				{
					flag = true;
					flag &= SPIflash.writeByte(teamFlashAddress + 16 + (uint32_t)block * (uint32_t)4 + (uint32_t)i * (uint32_t)4 + (uint32_t)0, ntag_page[0 + i * 4]);
					flag &= SPIflash.writeByte(teamFlashAddress + 16 + (uint32_t)block * (uint32_t)4 + (uint32_t)i * (uint32_t)4 + (uint32_t)1, ntag_page[1 + i * 4]);
					flag &= SPIflash.writeByte(teamFlashAddress + 16 + (uint32_t)block * (uint32_t)4 + (uint32_t)i * (uint32_t)4 + (uint32_t)2, ntag_page[2 + i * 4]);
					flag &= SPIflash.writeByte(teamFlashAddress + 16 + (uint32_t)block * (uint32_t)4 + (uint32_t)i * (uint32_t)4 + (uint32_t)3, ntag_page[3 + i * 4]);
					//if (flag) return false;
					checkCount++;
#ifdef DEBUG
					DebugSerial.print(F("write: "));
					for (uint8_t i = 0; i < 16; i++)DebugSerial.print(String(ntag_page[i], HEX) + " ");
					DebugSerial.println();
#endif
				}
				else
				{
#ifdef DEBUG
					DebugSerial.print(F("chip end: "));
					DebugSerial.println(String(page));
#endif
					block = TAG_MAX_PAGE;
					break;
			}
		}
	}
		block += 4;
}
	//add dump pages number
	if (checkCount > 0)
	{
		SPIflash.writeByte(teamFlashAddress + 12, checkCount & 0x000000FF);
		//SPIflash.writeByte(teamFlashAddress + 12, (checkCount & 0x0000FF00) >> 8);
		//SPIflash.writeByte(teamFlashAddress + 13, checkCount & 0x000000FF);
	}
	return flag;
}

//�������� �������� � ������� �� ����
bool readTeamFromFlash(uint16_t recordNum)
{
	uint32_t addr = (uint32_t)((uint32_t)recordNum * (uint32_t)LOG_RECORD_LENGTH);
	if (SPIflash.readByte(addr) == 0xff) return false;
	//#�������
	//����� �������������
	//�����
	//����� �������
	//������� ������� �� ����
	//for (uint8_t i = 0; i < 14; i++)
	for (uint8_t i = 0; i < 13; i++)
	{
		ntag_page[i] = SPIflash.readByte(addr + (uint32_t)i);
	}
	return true;
}

//������� ���������� � ���� �������
uint16_t refreshChipCounter()
{
	uint16_t chips = 0;

	uint32_t addr;
	for (uint16_t i = 1; i < LOG_LENGTH; i++)
	{
		addr = (uint32_t)((uint32_t)i * (uint32_t)LOG_RECORD_LENGTH);
		if (SPIflash.readByte(addr) != 255)
		{
			chips++;
			uint32_t time = SPIflash.readByte(addr + 8);
			time <<= 8;
			time += SPIflash.readByte(addr + 9);
			time <<= 8;
			time += SPIflash.readByte(addr + 10);
			time <<= 8;
			time += SPIflash.readByte(addr + 11);
			if (time > lastTimeChecked)
			{
				lastTimeChecked = time;
				lastTeams[0] = SPIflash.readByte(addr);
				lastTeams[1] = SPIflash.readByte(addr + 1);
			}
#ifdef DEBUG			
			DebugSerial.println(String(i));
#endif
			}
	}
#ifdef DEBUG
	DebugSerial.print(F("chip counter="));
	DebugSerial.println(String(chips));
#endif
	return chips;
		}

//��������� ������. ������������ ������ � ���������� � ������
void sendError(uint8_t errorCode, uint8_t commandCode)
{
	init_package(commandCode);
	uartBuffer[DATA_START_BYTE] = errorCode;
	uartBufferPosition = DATA_START_BYTE + 1;
	sendData();
}

//��������� ����� � ����� ��������� ������
void addLastTeam(uint16_t number)
{
	//����������� �����
	if (lastTeams[0] == (byte)(number >> 8) && lastTeams[1] == (byte)number) return;

	for (uint8_t i = lastTeamsLength * 2 - 1; i > 1; i = i - 2)
	{
		lastTeams[i] = lastTeams[i - 2];
		lastTeams[i - 1] = lastTeams[i - 3];
	}
	lastTeams[0] = (byte)(number >> 8);
	lastTeams[1] = (byte)number;

}

uint8_t crcCalc(uint8_t * dataArray, uint16_t startPosition, uint16_t dataEnd)
{
	uint8_t crc = 0x00;
	uint16_t i = startPosition;
	while (i <= dataEnd)
	{
		uint8_t tmpByte = dataArray[i];
		for (uint8_t tempI = 8; tempI; tempI--)
		{
			uint8_t sum = (crc ^ tmpByte) & 0x01;
			crc >>= 1;
			if (sum)
			{
				crc ^= 0x8C;
			}
			tmpByte >>= 1;
		}
		i++;
	}
	return (crc);
}

void floatToByte(byte * bytes, float f)
{
	int length = sizeof(float);

	for (int i = 0; i < length; i++)
	{
		bytes[i] = ((byte*)& f)[i];
	}
}

bool selectChipType(byte type)
{
	//#define NTAG213_MAX_PAGE	40; 0x12
	//#define NTAG215_MAX_PAGE	130; 0x3e
	//#define NTAG216_MAX_PAGE	226; 0x6d

	if (chipType == 0x12)
	{
		chipType = type;
		NTAG_MARK = 213;
		TAG_MAX_PAGE = 40;
	}
	else if (chipType == 0x6d)
	{
		chipType = type;
		NTAG_MARK = 216;
		TAG_MAX_PAGE = 226;
	}
	else
	{
		chipType = 0x3e;
		NTAG_MARK = 215;
		TAG_MAX_PAGE = 130;
		return false;
	}
	return true;
}