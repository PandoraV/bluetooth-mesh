/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
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
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define period_millis 1000 // 发信间隔
#define ADDRESS_PRESENT_SLAVE 1 // 从机地址位
#define info_num 2 // 传回上位机信息条数，测试时仅有温湿度两项
std::string info_name = "temp测试中文"; // 传回上位机的项目名称


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
        for (int i = 0; i < rxValue.length(); i++) // 则逐个输出
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};

void overFlow()
{
  // 溢出函数
  // TODO
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

  current_millis = millis();
  tempstr = std::to_string(current_millis); // 时间戳
  txValue += "\"c_mls\":";
  txValue += tempstr;
  txValue += ",";
  
  tempstr = std::to_string(info_num); // 条数
  txValue += "\"i_num\":";
  txValue += tempstr;
  txValue += ",";

  tempstr = info_name; // 项目名
  txValue += "\"i_name\":[";
  txValue += tempstr;
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

  current_millis = millis();
  send_millis = current_millis;
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
    }
    else if (current_millis < send_millis)
    {
      overFlow();
    }
	}

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) { 
    // 当前无设备连接，且ESP32记忆设备连接状态的标志位尚未更改，意思是已连接设备掉线了
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected; // 将已经记忆的状态更新
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // 新连接的设备进来之后对标志位进行更新
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
    Serial.println("device connected");
  }
}
