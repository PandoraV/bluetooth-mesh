## 基于蓝牙mesh网络的电气设备在线监测装置开发（程序部分）

* 2022/01/16
初步将工作划分为前端开发、上下位机通信开发和多蓝牙设备组网开发

# 文件目录
- wechatapp wx小程序代码
- code-esp32  esp32c3代码
- ESP32C3-I2C-Arduino 基于Arduino和ESP-IDF v4.4进行的I2C总线开发
    * 通过蓝牙向上位机发送信息
    * 采集传感器数据并传回
    * I2C控制OLED小屏