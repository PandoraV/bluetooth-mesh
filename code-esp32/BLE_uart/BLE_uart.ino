#include "DHT.h"

#define DHTPIN 2     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11   // DHT 11
// #define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor.
ulong dht_millis = 0; // DHT传感器采集时间
ulong dht_duration = 2500; // DHT采集间隔，实测约2300ms
float humidity;
float temperature;

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
#include <stdlib.h>

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false; // 当前有无设备连接
bool oldDeviceConnected = false; // 是否已经有设备连接

std::string txValue = "";

ulong current_millis = 0;
ulong send_millis = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 收信
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发信

ulong period_millis = 1000; // 发信间隔
#define ADDRESS_PRESENT_SLAVE 1 // 从机地址位
#define info_num 2 // 传回上位机信息条数，测试时仅有温湿度两项
// std::string info_name = "temp测试中文"; // 传回上位机的项目名称
std::string info_name[10] = {
  "temp",
  "humi",
  "NH3",
  "O3",
  "NO",
  "NO2"
};


class MyServerCallbacks: public BLEServerCallbacks {  // 调用成员函数修改设备连接状态
    void onConnect(BLEServer* pServer) { 
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks { // 将接收的字符串按字符逐个打印出来
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) { // 若接收到的字符串不为空
        Serial.println("*********");
        Serial.print("Received Value: ");
        int rx_len = rxValue.length();
        for (int i = 0; i < rx_len; i++) // 则逐个输出
          Serial.print(rxValue[i]);
        Serial.println();
        Serial.println("*********");

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
                    ulong hundred = 100;
                    new_period_millis = hundred*(rxValue[4] - '0');
                    for (int i = 5; i < rx_len; i++)
                    {
                      hundred /= 10;
                      new_period_millis += hundred*(rxValue[i] - '0');
                    }
                    Serial.print("the sensor period has been updated to ");
                    Serial.println(new_period_millis);
                    period_millis = new_period_millis;
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
                    new_period_millis += 1000*(rxValue[i] - '0');
                  }
                  if (new_period_millis != 0)
                  {
                    period_millis = new_period_millis;
                    Serial.print("the sensor period has been updated to ");
                    Serial.println(new_period_millis);
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
          // 发送文本
          break;
        
        default:
          break;
        }
      }
    }
};

void overFlow()
{
  // 溢出函数
}

void sendMsg(std::string msg_to_TX)
{
  // 将字符串发出去
  if (msg_to_TX.length() <= ESP_GATT_MAX_ATTR_LEN)
  {
    pTxCharacteristic->setValue(msg_to_TX);
    pTxCharacteristic->notify();
  }
  else{
    Serial.println("Length ERROR!");
    // 将超过六百字的字符串分两次发过去
    // TODO
  }
}

void setup_json_string()
{
  txValue = ""; // 清空txValue
  txValue += "{";

  std::string tempstr = "";

  // current_millis = millis();
  // tempstr = std::to_string(current_millis); // 时间戳
  // txValue += "\"c_mls\":";
  // txValue += tempstr;
  // txValue += ",";
  
  tempstr = std::to_string(info_num); // 条数
  txValue += "\"i_num\":";
  txValue += tempstr;
  txValue += ",";

  // tempstr = info_name; // 项目名
  txValue += "\"i_name\":[";
  for (int i = 0; i < info_num; i++)
  {
    // 将info_name列表里前info_num个名称添加进去
    tempstr = "\"";
    tempstr += info_name[i];
    tempstr += "\"";
    if (i != info_num - 1)
    {
      tempstr += ",";
    }
    txValue += tempstr;
  }
  txValue += "],";

  tempstr = std::to_string(period_millis); // 采样间隔
  txValue += "\"p_mls\":";
  txValue += tempstr;
  txValue += ",";

  tempstr = std::to_string(ADDRESS_PRESENT_SLAVE); // 当前从机地址
  txValue += "\"add\":";
  txValue += tempstr;
  txValue += ",";

  // 获取传感器数值
  // TODO
  tempstr = std::to_string(random(200,220)/10.0); // 温度
  txValue += "\"temp\":";
  txValue += tempstr;
  txValue += ",";

  tempstr = std::to_string(random(40,50)*1.0); // 湿度
  txValue += "\"humi\":";
  txValue += tempstr;
  // txValue += ",";

  txValue += "}";
}

void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("UART Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
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
}

void loop() {
  // 如果蓝牙发送过于频繁，会导致通信阻塞
  if (deviceConnected) {
		// delay(1000); // bluetooth stack will go into congestion, if too many packets are sent
    current_millis = millis();
    if (current_millis - send_millis >= period_millis)
    {
      send_millis = current_millis;
      setup_json_string();
      sendMsg(txValue);
      // Serial.println("string delivered:");
      // for (int i = 0; i < txValue.length(); i++) // 测试输出
      //   Serial.print(txValue[i]);
      // Serial.println();
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
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.print("%  Temperature: ");
      Serial.print(temperature);
      Serial.println("°C");
    }
  }
  else if (current_millis < dht_millis)
  {
    // overFlow();
    dht_millis = current_millis;
  }
}
