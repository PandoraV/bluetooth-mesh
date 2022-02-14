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
// #include <stdlib.h>

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false; // 当前有无设备连接
bool oldDeviceConnected = false; // 是否已经有设备连接

#define APPLE_REC 0
#define ANDROID_REC 1
int identity_verification = APPLE_REC; // 手机验证，0是苹果，1是安卓
bool deviceQueryed = false; // 当前是否已确认身份
bool duringDelivering = false;  // 是否处于发信函数调用状态
std::string txValue = "";  
std::string tx_str_for_query = "";

ulong current_millis = 0;
ulong send_millis = 0;    // 上次发信的时间
ulong queryDuration = 20; // 回信间隔
ulong period_millis = 1000; // 发信间隔

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 收信
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发信

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

void drawFontFace(void *parameter) {
  delay(1000);
  Serial.println("OLED thread started!");

  int i = 1;
  while(i == 1) {
    // clear the display
    display.clear();

    // create more fonts at http://oleddisplay.squix.ch/
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Hello world");
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, "Hello world");
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 26, "Hello world");

    // write the buffer to the display
    display.display();
    delay(100);
  }
  Serial.println("OLED thread ended.");
  vTaskDelete(NULL);
}

void overFlow()
{
  // 溢出函数
}

void sendMsg(std::string msg_to_TX)
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

void reply_for_query()
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
      sendMsg(tx_str_for_query);
      // TODO
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
                    new_period_millis += 1000*(rxValue[i] - '0'); // 毫秒为单位
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
          // 接收文本

          // 识别苹果安卓 
          if (rx_len == 5)
          {
            if (rxValue[1] == '0' && rxValue[2] == '0' && rxValue[3] == 'A')
            {
              if (rxValue[4] == 'P')
              {
                // APPLE
                identity_verification = APPLE_REC;
                deviceQueryed = true;
                Serial.println("current device has been recognized as APPLE!");
              }
              else if (rxValue[4] == 'N')
              {
                // Android
                identity_verification = ANDROID_REC;
                deviceQueryed = true;
                Serial.println("current device has been recognized as ANDORID!");
              }
            }
          }
          break;
        case 'Q': {   // 查询时间间隔
          if (rx_len == 4)
          {
            if (rxValue[1] == ADDRESS_PRESENT_SLAVE + '0')
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

void setup_json_string()
{
  txValue = ""; // 清空txValue

  if (identity_verification == APPLE_REC) // 为苹果生成
  {
    // 构建json
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
    if (info_num >= 2)
    {
      tempstr = std::to_string(temperature); // 温度
      txValue += "\"temp\":";
      txValue += tempstr;
      txValue += ",";

      tempstr = std::to_string(humidity); // 湿度
      txValue += "\"humi\":";
      txValue += tempstr;
      // txValue += ",";
    }

    txValue += "}";

  } else {
    // 为安卓
    uint8_t tempChar = 0;

    tempChar = '0' + info_num; // 数据条数
    txValue += tempChar;

    tempChar = '0' + ADDRESS_PRESENT_SLAVE; // 地址位
    txValue += tempChar;

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

        tempChar = (int)((temperature - (float)tempChar)*10);
        txValue += tempChar;
        // Serial.print("\ttemperature(LOW): ");
        // Serial.println((int)tempChar);

        // 再湿度
        tempChar = (int)humidity;
        txValue += tempChar;
        // Serial.print("\thumidity(HIGH): ");
        // Serial.print((int)tempChar);

        tempChar = (int)((humidity - (float)tempChar)*10);
        txValue += tempChar;
        // Serial.print("\thimidity(LOW): ");
        // Serial.println((int)tempChar);
      }
    }
  }
  
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
  
  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();

  xTaskCreate(
    drawFontFace,   /* Task function. */
    "TaskOne",      /* String with name of task. */
    10000,          /* Stack size in bytes. */
    NULL,           /* Parameter passed as input of the task */
    1,              /* Priority of the task. */
    NULL            /* Task handle. */
  );     

}

void loop() {
  // 如果蓝牙发送过于频繁，会导致通信阻塞
  if (deviceConnected && deviceQueryed) { // 当设备已连接且发送身份后，才返回
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
    deviceQueryed = false; // 清空设备种类记忆
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
