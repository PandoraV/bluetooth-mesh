#include <SoftwareSerial.h>
#define PZEM_BAUDRATE 9600
SoftwareSerial Serial2(2, 3); // 定义软件串口
void setup() {
  Serial.begin(115200);
  Serial2.begin(PZEM_BAUDRATE); // 初始化PZEM串口
}
void loop() {
  // 读取PZEM电压值、电流值、功率值
  float voltage = readPZEMData(0x04, 0x00, 0x00, 0x00, 0x00, 0x01, 2);
  float current = readPZEMData(0x04, 0x00, 0x01, 0x00, 0x00, 0x01, 2);
  float power = readPZEMData(0x04, 0x00, 0x03, 0x00, 0x00, 0x01, 2);
  // 发送电压值、电流值、功率值到上位机
  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.println(" V");
  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" A");
  Serial.print("Power: ");
  Serial.print(power);
  Serial.println(" W");
  delay(1000);
}
// 读取PZEM数据
float readPZEMData(byte cmd, byte regHi, byte regLo, byte lenHi, byte lenLo, byte num, byte len) {
  byte request[8] = {0xA1, cmd, regHi, regLo, lenHi, lenLo, 0x00, 0x00};
  byte response[7];
  // 计算CRC校验码
  uint16_t crc = crc16(request, 6);
  request[6] = crc >> 8;
  request[7] = crc & 0xFF;
  // 发送读取命令
  Serial2.write(request, 8);
  // 等待PZEM回复
  delay(100);
  // 读取PZEM回复数据
  int count = Serial2.available();
  if (count == 7 + num * len) {
    for (int i = 0; i < count; i++) {
      response[i] = Serial2.read();
    }
  }
  // 解析PZEM回复数据
  if (response[0] == 0xA1 && response[1] == cmd && response[2] == num * len) {
    float value = 0;
    for (int i = 0; i < num * len; i++) {
      value = value * 256 + response[3 + i];
    }
    return value / 10.0;
  } else {
    return -1;
  }
}
// 计算CRC校验码
uint16_t crc16(byte *buf, int len) {
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}
