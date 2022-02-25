#include "DHT.h"

#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor.
ulong dht_millis = 0; // DHT传感器采集时间
ulong dht_duration = 3000; // DHT采集间隔，实测约2300ms
float humidity = -1.0;     // 湿度
float temperature = -1.0;  // 温度

#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL 

// 软串口 
HardwareSerial mySerial1(1); // 软串口
uint8_t request_for_sensor_command[4][8] = {
  {0x01, 0x03, 0x00, 0x06, 0x00, 0x01, 0x64, 0x0B}, // 氨气传感器
  {0x02, 0x03, 0x00, 0x06, 0x00, 0x01, 0x64, 0x38}, // 臭氧传感器
  {0x03, 0x03, 0x00, 0x06, 0x00, 0x01, 0x65, 0xe9}, // NO传感器
  {0x04, 0x03, 0x00, 0x06, 0x00, 0x01, 0x64, 0x5e} // NO2传感器
};

uint8_t data_high_byte[4] = {0x00, 0x00, 0x00, 0x00}; // 数据高八位
uint8_t data_low_byte[4]  = {0x00, 0x00, 0x00, 0x00}; // 数据低八位

/*
  Ported to Arduino ESP32 by Evandro Copercini

  The design of creating the BLE server is:
  1. Create a BLE Server
  2. Create a BLE Service
  3. Create a BLE Characteristic on the Service
  4. Create a BLE Descriptor on the characteristic
  5. Start the service.
  6. Start advertising.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string.h>

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false; // 当前有无设备连接
bool oldDeviceConnected = false; // 是否已经有设备连接

bool duringDelivering = false;  // 是否处于发信函数调用状态
std::string txValue = "";  
std::string tx_str_for_query = "";

#define DEFAULT_PERIOD_MILLIS 1000
#define MINIMUM_PERIOD_MILLIS 500
#define MAXIMUM_PERIOD_MILLIS 65535
#define SENSOR_OVERTIME_MILLIS 10
ulong current_millis = 0;
ulong send_millis = 0;    // 上次发信的时间
ulong queryDuration = 20; // 回信间隔
ulong period_millis = DEFAULT_PERIOD_MILLIS; // 发信间隔

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 收信
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发信

#define ADDRESS_PRESENT_SLAVE 1 // 从机地址位
#define info_num 4 // 传回上位机信息条数，测试时仅有温湿度两项
// std::string info_name = "temp测试中文"; // 传回上位机的项目名称
String info_name[6] = {  "temp",  "humi",  "NH3",  "O3",  "NO",  "NO2"};

uint16_t GetCRC16(uint8_t *ptr, uint8_t len)
{
  uint16_t index;
  uint8_t crch = 0xFF; // 高 CRC 字节 
  uint8_t crcl = 0xFF; // 低 CRC 字节
  uint8_t TabH[] = { // CRC 高位字节值表
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 
    0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 
    0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 
    0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 
    0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 
    0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 
    0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 
    0xC1, 0x81, 0x40
  };
  uint8_t TabL[] = { // CRC 低位字节值表
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 
    0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 
    0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 
    0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 
    0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 
    0xD6, 0xD2, 0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 
    0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 
    0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 
    0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 
    0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22, 
    0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 
    0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 
    0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 
    0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 
    0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 
    0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 
    0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 
    0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 
    0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 
    0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, 0x44, 0x84, 
    0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 
    0x81, 0x80, 0x40
  };
  while(len--) //计算指定长度的CRC 
  {
    index = crch ^ *ptr++; 
    crch = crcl ^ TabH[index]; 
    crcl = TabL[index];
  }
  return ((crch << 8) | crcl);
}

void drawFontFace(void *parameter) { // OLED驱动函数
  delay(1000);
  Serial.println("OLED thread started!");

  ulong temp_num = 0;

  int i = 1;
  while(i == 1) {
    // clear the display
    display.clear();

    // create more fonts at http://oleddisplay.squix.ch/
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "temp: " + String(temperature) + "°C");
    display.drawString(0, 16, "humi: " + String(humidity) + "%");
    
    // 屏幕刷新帧数
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 54, String(temp_num));
    temp_num++;

    // write the buffer to the display
    display.display();
    delay(100);
  }
  Serial.println("OLED thread ended."); // 不会执行
  vTaskDelete(NULL); // 删除线程 释放内存
}

void overFlow()
{
  // 溢出函数
}

void gas_sensor_serial(void *parameter) // 气体传感器软串口
{
  Serial.println("sensor thread started!");

  int i = 1;
  while(i == 1) { // 死循环
    std::string data = ""; 

    // 将已存储的传感器数据位清空
    for (int i = 0; i < info_num - 2; i++) {
      data_high_byte[i] = 0x00;
      data_low_byte[i]  = 0x00;
    }

    for (int i = 0; i < info_num - 2; i++) { // 轮询
      mySerial1.write(request_for_sensor_command[i], 8); // 发送测温命令
      delay(SENSOR_OVERTIME_MILLIS);  // 等待测温数据返回
      
      if (mySerial1.available() > 0) {
        // 有返回数据
        // Serial.println("the " + String(i) + " has replyed!");
      } else {
        // 无返回数据
        data_high_byte[i] = 0xff;
        data_low_byte[i]  = 0xff;
        // Serial.print(i);
        // Serial.println(": the sensor didn't reply anything!");
        continue;
      }

      data = ""; // 清空
      while (mySerial1.available()) { // 从串口中读取数据
        uint8_t in = (uint8_t)mySerial1.read();  // read读取
        // Serial.print(in, HEX);
        // Serial.print(' ');
        data += in;
      }
      // Serial.println();

      if (data.length() > 0) { 
        // 校验操作

        // 首先确认是否是7位数据
        if (data.length() == 7) {
          // 数据位符合要求，检查CRC16
          uint8_t data_char_str[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
          for (int j=0; j < 7; j++) { // 将string类型字符串传入unsigned char
            data_char_str[j] = data[j];
          }
          uint16_t crcInt = GetCRC16(data_char_str, 5);
          unsigned char crcl = crcInt;
          unsigned char crch = crcInt >> 8;
          if (crch == data[5] && crcl == data[6]) {
            // 校验通过
            // Serial.println("received data has passed the verification and has been authorized.");
            data_high_byte[i] = data[3];
            data_low_byte[i]  = data[4];
            Serial.print("the data bit of " + info_name[i + 3] + " is ");
            Serial.print(data_high_byte[i], HEX);
            Serial.print(" ");
            Serial.println(data_low_byte[i], HEX);
          } else {
            // 数据异常
            Serial.println("data loss! The data received has not passed the CRC verification!");
            data_high_byte[i] = 0xff;
            data_low_byte[i]  = 0xff;
          }
        } else {
          // 数据位数异常
          Serial.println("data loss! the length of data doesn't equal 7");
          data_high_byte[i] = 0xff;
          data_low_byte[i]  = 0xff;
        }
      }
    }
    delay(period_millis);
  }
  Serial.println("sensor thread ended."); // 不会执行
  vTaskDelete(NULL); // 删除线程 释放内存
}

void sendMsg(std::string msg_to_TX) // 蓝牙发送信息函数
{
  // 修改标志位，表明当前处于发信状态中
  duringDelivering = true;

  // 将字符串发出去
  int msg_to_TX_length = msg_to_TX.length();
  if (msg_to_TX_length <= ESP_GATT_MAX_ATTR_LEN)
  {
    pTxCharacteristic->setValue(msg_to_TX);
    pTxCharacteristic->notify();

    // Serial.println("string delivered:");
    // for (int i = 0; i < msg_to_TX_length; i++) // 测试输出
    //   Serial.print(msg_to_TX[i]);
    // Serial.println();
  }
  else{
    Serial.println("Length ERROR!");
    // 将超过六百字的字符串分多次发过去，仅对苹果
  }

  // 结束发信
  duringDelivering = false;
}

void reply_for_query() // 查询命令回信
{
  // 检查是否可调用发信函数

  // 检查当前发信函数是否被调用
  if (!duringDelivering) // 不在发送中
  {
    // 检查前后时间是否合适
    current_millis = millis();
    if (current_millis - send_millis >= queryDuration && 
        current_millis - send_millis <= period_millis - queryDuration)
    {
      sendMsg(tx_str_for_query);
    } else {
      // 等待
      // TODO
      sendMsg(tx_str_for_query);
      // 应设置独立线程等待到可以发信
    }
  }
}

class MyServerCallbacks: public BLEServerCallbacks {  // 调用成员函数修改设备连接状态
    void onConnect(BLEServer* pServer) { 
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks { // 处理接收的字符串
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) { // 若接收到的字符串不为空
        Serial.print("Received Value: ");
        int rx_len = rxValue.length();
        for (int i = 0; i < rx_len; i++) // 则逐个输出
          Serial.print(rxValue[i]);
        Serial.println();

        // 判断命令合法性
        if (rx_len < 4) // 是否包含全部关键位
        {
          Serial.println("the command is too short to be right");
        }
        else if (rxValue[1] > '9' || rxValue[1] < '0') // 传感器编号格式是否正确
        {
          Serial.println("the command is illegal at sensor site");
        }
        else if (rxValue[1] > '9' || rxValue[1] < '0') // 从机地址格式是否正确
        {
          Serial.println("the command is illegal at address site");
        }

        // 对接受到的命令进行处理
        switch (rxValue[0])
        {
        case 'P':
          // 调整采样间隔
          switch (rxValue[1])
          {
          case '0':// 不指定传感器
            {
              // 看第三位地址位
              int add_received;
              add_received = rxValue[2] - '0';
              if (add_received == ADDRESS_PRESENT_SLAVE)
              {
                // 取数
                // 看是不是小数
                if (rxValue[3] == '0')
                {
                  // 看有没有第五位
                  if (rx_len == 4)
                  {
                    Serial.println("the command is illegal at data site");
                  } else {
                    ulong new_period_millis = 0;
                    ulong hundred = 100; // 毫秒为单位
                    new_period_millis = hundred*(rxValue[4] - '0');
                    for (int i = 5; i < rx_len; i++)
                    {
                      hundred /= 10;
                      new_period_millis += hundred*(rxValue[i] - '0');
                    }
                    if (new_period_millis < MINIMUM_PERIOD_MILLIS)
                    {
                      Serial.println("the new period " + String(new_period_millis) + " is smaller than the minimum value");
                    } else {
                      Serial.print("the sensor period has been updated to ");
                      Serial.println(new_period_millis);
                      period_millis = new_period_millis;
                    }
                  }
                } else {
                  // 整数秒
                  ulong new_period_millis = 0;
                  for (int i=3; i<rx_len; i++)
                  {
                    if (rxValue[i] > '9' || rxValue[i] < '0')
                    {
                      Serial.println("the command is illegal at data site");
                      new_period_millis = 0;
                      break;
                    }
                    new_period_millis *= 10; // 进位
                    new_period_millis += rxValue[i] - '0'; // 毫秒为单位
                  }
                  if (new_period_millis != 0)
                  {
                    if (new_period_millis > MAXIMUM_PERIOD_MILLIS)
                    {
                      Serial.println("the new period " + String(new_period_millis) + " is larger than the maximum value");
                    } else {
                      period_millis = new_period_millis;
                      Serial.print("the sensor period has been updated to ");
                      Serial.println(new_period_millis);
                    }
                  }
                }
              }
              break;
            }
          case '1':// 氨气传感器
            break;
          case '2':// 臭氧传感器
            break;
          case '3':// 一氧化氮传感器
            break;
          case '4':// 二氧化氮传感器
            break;
          case '5':// 温湿度传感器
            break;
          
          default: // 该编号无对应传感器！
            Serial.println("Error! The sensor is not be recorded");
            break;
          }
          break;
        case 'T':
          // 接收文本
          break;
        case 'Q': {   // 查询时间间隔
          if (rx_len == 4)
          {
            if (rxValue[2] == ADDRESS_PRESENT_SLAVE + '0')
            {
              // 清空发送字符串
              tx_str_for_query = "";
              uint8_t tempChar = 1; // 发信数量为1，表明这是传回时间间隔
              tx_str_for_query += tempChar + '0'; 
              tempChar = ADDRESS_PRESENT_SLAVE; // 地址位
              tx_str_for_query += tempChar + '0';
              std::string tempStr = "";
              tempStr = std::to_string(period_millis);
              tx_str_for_query += tempStr;
              Serial.print("current period millis is ");
              Serial.println(period_millis);

              reply_for_query();
            }
          }
        }
          break;
        default:
          break;
        }
      }
    }
};

void setup_json_string() // 构建发送的json字符串
{
  txValue = ""; // 清空txValue

  uint8_t tempChar = 0;

  tempChar = '0' + info_num; // 数据条数
  txValue += tempChar;

  tempChar = '0' + ADDRESS_PRESENT_SLAVE; // 地址位
  txValue += tempChar;

  // 时间间隔
  tempChar = period_millis / 256;
  txValue += tempChar;
  // Serial.print("plm high byte:");
  // Serial.print((int)tempChar);
  tempChar = period_millis - tempChar*256;
  txValue += tempChar;
  // Serial.print("\tplm low byte:");
  // Serial.println((int)tempChar);

  if (info_num >= 2) // 温湿度传感器
  {
    if (humidity < 0 || temperature < 0)
    {
      tempChar = 0;
      for (int i=0; i<4; i++)
        txValue += tempChar;
    }
    else {
      // 先温度
      tempChar = (int)temperature;
      txValue += tempChar;
      // Serial.print("temperature(HIGH): ");
      // Serial.print((int)tempChar);

      tempChar = (temperature*10 - tempChar*10);
      txValue += tempChar;
      // Serial.print("\ttemperature(LOW): ");
      // Serial.println((int)tempChar);

      // 再湿度
      tempChar = (int)humidity;
      txValue += tempChar;
      // Serial.print("\thumidity(HIGH): ");
      // Serial.print((int)tempChar);

      tempChar = (humidity*10 - tempChar*10);
      txValue += tempChar;
      // Serial.print("\thimidity(LOW): ");
      // Serial.println((int)tempChar);
    }
  }
  // 其他传感器
  if (info_num > 2)
  {
    if (info_num >= 3)
    {
      // 氨气传感器
      txValue += data_high_byte[0];
      txValue += data_low_byte[0];
    }
    if (info_num >= 4)
    {
      // 臭氧传感器
      txValue += data_high_byte[0];
      txValue += data_low_byte[0];
    }
    if (info_num >= 5)
    {
      // NO传感器
      txValue += data_high_byte[0];
      txValue += data_low_byte[0];
    }
    if (info_num >= 6)
    {
      // NO2传感器
      txValue += data_high_byte[0];
      txValue += data_low_byte[0];
    }
  }
  
}

void setup() {
  Serial.begin(115200);

  BLEDevice::init("UART Service");                // Create the BLE Device

  pServer = BLEDevice::createServer();            // Create the BLE Server
  pServer->setCallbacks(new MyServerCallbacks()); // 将连接状态响应函数设置为第一个类

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID); // 设置设备的唯一蓝牙标识码

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX, // 发信通道
										BLECharacteristic::PROPERTY_NOTIFY // 发信方法为notify
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX, // 收信通道
											BLECharacteristic::PROPERTY_WRITE // 收信方法为write
										);

  pRxCharacteristic->setCallbacks(new MyCallbacks()); // 设置响应函数

  // Start the service
  pService->start(); // 启动服务

  // Start advertising
  pServer->getAdvertising()->start(); // 开始广播蓝牙
  Serial.println("Waiting a client connection to notify...");

  dht.begin(); // 启动DHT检测类
  Serial.println("DHT11 started!");

  current_millis = millis();
  send_millis = current_millis;
  dht_millis = current_millis; // 需要等待时间让DHT传感器启动
  
  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();

  xTaskCreate(
    drawFontFace,   /* Task function. */
    "Display",      /* String with name of task. */
    10000,          /* Stack size in bytes. */
    NULL,           /* Parameter passed as input of the task */
    1,              /* Priority of the task. */
    NULL            /* Task handle. */
  );

  // 软串口启动
  mySerial1.begin(9600, SERIAL_8N1);

  xTaskCreate(
    gas_sensor_serial,   /* Task function. */
    "Display",      /* String with name of task. */
    10000,          /* Stack size in bytes. */
    NULL,           /* Parameter passed as input of the task */
    1,              /* Priority of the task. */
    NULL            /* Task handle. */
  );
}

void loop() {
  // 如果蓝牙发送过于频繁，会导致通信阻塞
  if (deviceConnected) { // 当设备已连接且发送身份后，才返回
		// delay(1000); // bluetooth stack will go into congestion, if too many packets are sent
    current_millis = millis();
    if (current_millis - send_millis >= period_millis)
    {
      send_millis = current_millis;
      setup_json_string();
      sendMsg(txValue);
    }
    else if (current_millis < send_millis)
    {
      // overFlow();
      send_millis = current_millis;
    }
	}
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) { 
    // 当前无设备连接，且ESP32记忆设备连接状态的标志位尚未更改，意思是已连接设备掉线了
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("restart advertising");
    oldDeviceConnected = deviceConnected; // 将已经记忆的状态更新
    period_millis = DEFAULT_PERIOD_MILLIS; // 恢复默认采样时间间隔
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // 新连接的设备进来之后对标志位进行更新
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
    Serial.println("device connected");
  }

  // DHT
  current_millis = millis();
  if (current_millis - dht_millis >= dht_duration)
  {
    // 读取
    dht_millis = current_millis;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) { // 若返回为空值
      Serial.println("Failed to read from DHT sensor!");
      humidity = -1.0;
      temperature = -1.0;
    } else {
      humidity = h;
      temperature = t;
      // Serial.print("Humidity: ");
      // Serial.print(humidity);
      // Serial.print("%  Temperature: ");
      // Serial.print(temperature);
      // Serial.println("°C");
    }
  }
  else if (current_millis < dht_millis)
  {
    // overFlow();
    dht_millis = current_millis;
  }
}
