# ESP32C3下位机驱动程序

## 一、基本介绍

### 本程序使用ESP32C3的功能

- （1）通过`BLE`（低功耗蓝牙），向上位机发送当前监测传感器的实时数值，
包括`氨气`、`氮氧化物（NO和NO2）`、`臭氧`三种，及`温湿度`；
    
- （2）驱动0.96寸OLED小屏显示数值。

### 本装置用到的硬件

- 核心电路板：ESP-C3-32S-kit 3297
- 温湿度传感器：`DHT11`，量程湿度`5~95%RH`， 温度`-20~60℃`，驱动电压与单片机逻辑电平相同（`5V`/`3.3V`）
- `0.96`寸OLED小屏：型号`SSD1306`，分辨率`128*64`，驱动电压`3.3V`
- 氨气传感器：`0-1000ppm`，分辨率分段变化，在0~250ppm内为`0.01ppm`，在250ppm以上（含）是`0.1ppm`，驱动电压`5V`，下同
- 臭氧传感器：`0-100ppm`，分辨率`0.01ppm`
- 一氧化氮传感器：`0-2000ppm`，分辨率分段变化，在0~250ppm内为`0.01ppm`，在250ppm以上（含）是`0.1ppm`
- 二氧化氮传感器：`0-2000ppm`，分辨率分段变化，在0~250ppm内为`0.01ppm`，在250ppm以上（含）是`0.1ppm`

后四个有害气体传感器采购自精讯畅通厂家直销店，负责人为魏经理，售后为刘工。各个硬件的详细引脚定义见技术手册。

### 本程序用到的样例

全部引用样例如下，部分样例在工程目录文件下有备份。

- 乐鑫Arduino-ESP32仓库的示例代码 BLE_uart
- ThingPulse公司`ESP8266 and ESP32 OLED`库的OLED样例代码 SSD1306SimpleDemo
- Layada的DHT驱动库 DHTteste
- CSDN社区@cyw_2018 ESP32多线程
- CSDN社区@xiaoshihd ESP32+RS485测量教程
- CSDN社区@青睚白鹿 ESP32连接RS485土壤湿度传感器上传至MySQL

参考链接分别为：

- https://github.com/espressif/arduino-esp32
- https://github.com/ThingPulse/esp8266-oled-ssd1306
- https://github.com/adafruit/DHT-sensor-library
- https://blog.csdn.net/qq_41595493/article/details/109710067
- https://blog.csdn.net/xiaoshihd/article/details/109398264
- https://blog.csdn.net/weixin_42618434/article/details/116019430

其中，DHT的包正常运行需要依赖于Adafruit的Unified Sensor Lib: 
- https://github.com/adafruit/Adafruit_Sensor

## 二、环境配置

### Arduino

Arduino IDE开发ESP32的环境配置：

- 开发板网址加入 https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

- 搜索ESP32，安装此包。本程序基于2.0.2版本开发，其兼容v4.4的ESP-IDF。

### DHT传感器

DHT传感器的硬件连线和定义：

``` C++
#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTTYPE DHT21   // DHT 21 (AM2301)
```

将传感器有孔道的那一面对着自己，最左边的是VCC，最右边是GND，左数第二个是数据引脚。VCC与逻辑电平TTL相同，ESP32逻辑电平为5V

### OLED小屏

OLED小屏背后有地址选址的电阻，提供两个不同地址位选择。目测测试物理地址位与程序地址位不相符。

OLED声明定义：

``` C++
// Include the correct display library

// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
OR #include "SH1106Wire.h"   // legacy: #include "SH1106.h"

// For a connection via I2C using brzo_i2c (must be installed) include:
#include <brzo_i2c.h>        // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Brzo.h"
OR #include "SH1106Brzo.h"

// For a connection via SPI include:
#include <SPI.h>             // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Spi.h"
OR #include "SH1106SPi.h"
```

OLED初始化与连线：

```C++
// Initialize the OLED display using Arduino Wire:
SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL  
SSD1306Wire display(0x3c, D3, D5);  // ADDRESS, SDA, SCL  -  If not, they can be specified manually.
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32); 
                                    // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.
SH1106Wire display(0x3c, SDA, SCL);     // ADDRESS, SDA, SCL

// Initialize the OLED display using brzo_i2c:
SSD1306Brzo display(0x3c, D3, D5);  // ADDRESS, SDA, SCL
or
SH1106Brzo display(0x3c, D3, D5);   // ADDRESS, SDA, SCL

// Initialize the OLED display using SPI:
D5 -> CLK
D7 -> MOSI (DOUT)
D0 -> RES
D2 -> DC
D8 -> CS
SSD1306Spi display(D0, D2, D8);  // RES, DC, CS
or
SH1106Spi display(D0, D2);       // RES, DC
```

OLED常用基本函数：

```C++
// clear the display 清空已有显示
display.clear();

// create more fonts at http://oleddisplay.squix.ch/
display.setTextAlignment(TEXT_ALIGN_LEFT); // 显示位置，可选参数包括LEFT，RIGHT和CENTER，详见同目录下OLED示例程序
display.setFont(ArialMT_Plain_16); // 设置字体和字体大小，尾数即为字体大小
display.drawString(0, 0, "temp: " + String(temperature) + "°C"); // 前两个参数为绘制文本的起始位置的x，y坐标

// write the buffer to the display
display.display(); // 将上述函数构成
```

本项目选择的是四引脚的OLED小屏，使用I2C总线通信，使用的地址位是0x3c，使用第一种初始化方式。

后续的SDA和SCL引脚在开发板提供者的引脚定义头文件中写明，pins_arduino.h 对于ESP32C3，两个引脚分别为引脚是**8（SDA）**和**9（SCL）**，分别对应**GPIO8**和**GPIO9**。

- Ref: https://github.com/espressif/arduino-esp32/blob/master/variants/esp32c3/pins_arduino.h

**值得注意**的是，Arduino中ESP32的GPIO口的标号对应其在程序中调用的数字。

### 有害气体传感器

使用TTL通信或485通信，波特率2400/4800/9600可选，出厂默认9600。外部设置金属网对电磁干扰进行屏蔽。使用modbus-RTU通信协议，与ESP通信须使用485转TTL模组，具体通信格式见通信协议。

#### 传感器使用注意事项：

- 不要直接焊接模组的插针，可以焊接插针的管座
- 不可经受过度撞击和震动
- 初次上电使用需预热三分钟

#### 与传感器的通信：

与传感器的通信使用软串口通信，在ESP8266中须引用`<SoftwareSerial.h>`头文件，在ESP32中不须引用头文件，已经默认在core中编译了头文件`<HardwareSerial.h>`，在代码中引用会报错。ESP的串口可以另外指定引脚，而不一定强制使用官方封装的引脚；但8266和32的引脚声明又略有区别。

在ESP8266中定义如下，`SoftwareSerial`类的初始化传入参数包括了TTL通信的两个引脚。

```C++
#include <SoftwareSerial.h>
SoftwareSerial tempSerial(8, 7);  // RX, TX，注意顺序
```

8266的软串口使用：

```C++
void setup()
{
  tempSerial.begin(9600);
}

void loop()
{
    tempSerial.listen();  // 监听温度串口
    for (int i = 0 ; i < 8; i++) {  // 发送测温命令
        tempSerial.write(tempCommand[i]);   // write输出
    }
    delay(100);  // 等待测温数据返回
    tempData = "";
    while (tempSerial.available()) {//从串口中读取数据
        uint8_t in = (uint8_t)tempSerial.read();  // read读取, unsigned char define成 uint8_t
        Serial.print(in, HEX);
        Serial.print(',');
        tempData += in;
        tempData += ',';
    }
    tempSerial.end();
}
```
其中，开始监听的`listen`函数和`end`函数可不写，且是ESP8266独占函数，在ESP32中不可使用。

ESP32中定义如下，传入参数`1`表明板子上总共有几路可使用的串口通信引脚。

```C++
// #include <HardwareSerial.h> // 引用的头文件，可以在他的cpp里看具体的函数接口定义
HardwareSerial mySerial1(1); //软串口，用来与传感器进行通信
```

在ESP中，不可在这一步中指定使用别的引脚，需要在串口启动函数中才能另外指定。

```C++
mySerial1.begin(4800,SERIAL_8N1,35,12); 
```

对于具体的参数进行说明：

- `4800`：波特率
- `SERIAL_8N1`：表设置`config`，表示8位数据位，无校验位，1位停止位。
- 后两个参数分别为`rxPin`和`txPin`的引脚标号

本装置中使用的`TX`、`RX`的引脚与ESP32C3官方封装的串口通信引脚一致，为`IO20`（`RX`）和`IO21`（`TX`）。上面后三个参数不需要特别说明，`config`和`TX`、`RX`默认值均已在头文件里写好。

具体的读写方式与上面ESP8266除了不需要`listen`和`end`之外完全一致。

#### 485转TTL模组

可能使用的是MAX485芯片为核心的电路，一侧是电源+地+TX和RX，另一侧接485，是GND和485-A与485-B。芯片TX接ESP单片机RX，芯片RX接ESP的TX，而485则是芯片与设备A与A相连，B与B相连。

![485toTTL](485toTTL.jpeg)

## 三、编程注意事项

### 蓝牙BLE_uart

新建蓝牙的步骤为：

```C++
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
```

需要指定蓝牙的设备唯一ID和他的发信收信通道ID：

```C++
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // 收信
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // 发信
```

需要引用的头文件和初始化的类包括：

```C++
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
// #include <string.h> 

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false; // 当前有无设备连接
bool oldDeviceConnected = false; // 是否已经有设备连接

class MyServerCallbacks: public BLEServerCallbacks {} // 设备状态更改
class MyCallbacks: public BLECharacteristicCallbacks {} // 收信响应类
```

更具体的操作请看样例代码。

其他注意事项：

- ESP蓝牙模块读写不支持Arduino的`String`关键字，仅可以使用C++自带的`<string.h>`头文件里的`string`类型，调用时须指明是`std`命名空间里的，如`std:string`

### OLED驱动

因为OLED不可以无限制快速刷新，每次刷新中间需要时间间隔，因此将其放在单独的线程中刷新。

#### ESP的线程

使用封装的函数`xTaskCreate`。示例代码：

```C++
xTaskCreate(
    taskOne,   /* Task function. */
    "TaskOne", /* String with name of task. */
    10000,     /* Stack size in bytes. */
    NULL,      /* Parameter passed as input of the task */
    1,         /* Priority of the task. */
    NULL     /* Task handle. */
);
```

第一个参数为要在线程中执行的函数的函数名，第三个参数为给该线程分配的栈的字节数（即内存大小），第五个参数为优先级。当第四个参数，传入函数的参数，设定为`NULL`时，相对应的函数应当写明函数不接受任何参数，如下：

```C++
void taskOne(void *parameter) {}
```

应特别注明接受参数为`void *parameter`，而写作`void`不够严格，在编译器里会报错。

### DHT驱动

DHT不可写在线程里，必须在主线程中不断循环，且其采样时间不随外部输入时间确定。