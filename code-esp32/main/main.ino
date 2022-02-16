/*
  main.ino
  基于ESP32C3的有害气体在线监测装置驱动软件

  Created by 张頔 on 2022/2/11.
  Copyright © 张頔·中国农业大学 个人版权所有 All rights reserved.

  -----------------------------------------------------------------------
  修订历史/Revision History  
  日期/Date    作者/Author      参考号/Ref    修订说明/Revision Description
  2022/2/11       张頔             1.0               初始版本
  -----------------------------------------------------------------------

  本程序功能包括：
  （1）通过BLE（低功耗蓝牙），向上位机发送当前监测传感器的实时数值，
        包括氨气、氮氧化物（NO和NO2）、臭氧三种，及温湿度；
  （2）驱动0.96寸OLED小屏显示数值。

  本装置用到的硬件包括：
  - 温湿度传感器：DHT11，驱动电压与单片机逻辑电平相同（5V/3.3V）
  - 0.96寸OLED小屏：型号SSD1306，分辨率128*64，驱动电压3.3V
  - 氨气传感器：0-1000ppm，分辨率0.1ppm
  - 臭氧传感器：0-100ppm，分辨率0.1ppm
  - 一氧化氮传感器：0-2000ppm，分辨率0.1ppm
  - 二氧化氮传感器：0-2000ppm，分辨率0.1ppm
  后四个有害气体传感器采购自精讯畅通厂家直销店，负责人为魏经理。

  本程序用到的样例包括：
  - 乐鑫Arduino-ESP32仓库的示例代码 BLE_uart
  - ThingPulse公司ESP8266 and ESP32 OLED库的OLED样例代码 SSD1306SimpleDemo
  - Layada的DHT驱动库 DHTtester
  参考链接分别为：
  - https://github.com/espressif/arduino-esp32
  - https://thingpulse.com
  - https://github.com/adafruit/DHT-sensor-library
  其中，DHT的包正常运行需要依赖于Adafruit的Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

  Arduino开发ESP32的环境配置：
  - 开发板网址加入 https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  - 搜索ESP32，安装此包。本程序基于2.0.2版本开发，其兼容v4.4的ESP-IDF。

  DHT传感器的硬件连线和定义：
  ```
  #define DHTTYPE DHT11   // DHT 11
  #define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
  #define DHTTYPE DHT21   // DHT 21 (AM2301)
  ```
  将传感器有孔道的那一面对着自己，最左边的是VCC，最右边是GND，左数第二个是数据引脚。VCC与逻辑电平TTL相同，ESP32逻辑电平为5V

  OLED小屏背后有地址选址的电阻，提供两个不同地址位选择。目测测试物理地址位与程序地址位不相符。
  OLED声明定义：

  ```
  // Include the correct display library

  For a connection via I2C using the Arduino Wire include:
  #include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
  #include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
  OR #include "SH1106Wire.h"   // legacy: #include "SH1106.h"

  For a connection via I2C using brzo_i2c (must be installed) include:
  #include <brzo_i2c.h>        // Only needed for Arduino 1.6.5 and earlier
  #include "SSD1306Brzo.h"
  OR #include "SH1106Brzo.h"

  For a connection via SPI include:
  #include <SPI.h>             // Only needed for Arduino 1.6.5 and earlier
  #include "SSD1306Spi.h"
  OR #include "SH1106SPi.h"
  ```

  OLED初始化与连线：

  ```
  Initialize the OLED display using Arduino Wire:
  SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL  
  SSD1306Wire display(0x3c, D3, D5);  // ADDRESS, SDA, SCL  -  If not, they can be specified manually.
  SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32); 
                                     // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.
  SH1106Wire display(0x3c, SDA, SCL);     // ADDRESS, SDA, SCL

  Initialize the OLED display using brzo_i2c:
  SSD1306Brzo display(0x3c, D3, D5);  // ADDRESS, SDA, SCL
  or
  SH1106Brzo display(0x3c, D3, D5);   // ADDRESS, SDA, SCL

  Initialize the OLED display using SPI:
  D5 -> CLK
  D7 -> MOSI (DOUT)
  D0 -> RES
  D2 -> DC
  D8 -> CS
  SSD1306Spi display(D0, D2, D8);  // RES, DC, CS
  or
  SH1106Spi display(D0, D2);       // RES, DC
  ```

  本项目选择的是四引脚的OLED小屏，使用I2C总线通信，使用的地址位是0x3c，使用第一种初始化方式。
  后续的SDA和SCL引脚在开发板提供者的引脚定义头文件中写明，pins_arduino.h 对于ESP32C3，两个引脚分别为引脚是8（SDA）和9（SCL），分别对应GPIO8和GPIO9。
  Ref: https://github.com/espressif/arduino-esp32/blob/master/variants/esp32c3/pins_arduino.h
  **值得注意**的是，Arduino中ESP32的GPIO口的标号对应其在程序中调用的数字。

  蓝牙BLE_uart开发注意事项：
  ESP32不兼容Arduino本身的一些库，由于其使用C++进行开发，一些关键词的调用需指明命名空间。
  对于字符串string关键字，首先ESP不支持Arduino的String关键字，其次调用时须指明是std命名空间里的，如std:string
  实测支持Arduino自己的delay()、millis()等原生函数。

  根据开源协议要求，本程序保留所参考部分样例相关版权信息注释。

   The MIT License (MIT)

   Copyright (c) 2018 by ThingPulse, Daniel Eichhorn
   Copyright (c) 2018 by Fabrice Weinberg

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   ThingPulse invests considerable time and money to develop these open source libraries.
   Please support us by buying our products (and not the clones) from
   https://thingpulse.com
   
*/
  