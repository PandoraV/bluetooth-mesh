# 微信小程序蓝牙控制ESP32-C3
# 一、前言


## 测试设备

| 手机型号  | 系统版本  | 微信版本   | 基础库版本       |
|:---------:|:---------:|:------:|:-----------:|
| iPhone SE | IOS 12.4 | 7.0.10 | 2.10.4[526] |
| iPhone SE | IOS 12.1.2 | 8.0.14 |  |
| 华为 mate 20 pro(UD) | 鸿蒙 2.0.0.209 | 8.0.14 |  |

下位机：ESP32

## 特点

- [x] [无障碍访问](https://developers.weixin.qq.com/miniprogram/dev/component/aria-component.html)
- [] [数据可视化](https://github.com/ecomfe/echarts-for-weixin)

# 二、设备核心代码

## 2.1 蓝牙控制

Arduino ESP32 BLE_uart

# 三、微信小程序核心代码

- [微信小程序蓝牙功能 独孤求赞 于 2021-01-25](https://blog.csdn.net/weixin_44989478/article/details/112845696):程序不可直接使用，程序逻辑上没问题，但是存在bug，需要稍加修改
- [微信小程序怎么实现蓝牙连接？（代码示例）青灯夜游 转载2018-11-13](https://www.php.cn/xiaochengxu-412851.html)：以此为蓝本修改上面的代码
- []()
- []()

# 四、页面设计

- 设备搜索页：显示周围的所有设备，是否已连接
- 设备状态实时状态页：展示设备当前监测数据，具备指令操作功能
- 设备历史数据记录页：展示历史数据，提供保存接口

由 Page 过渡到 Tab

# 五、FAQ

## notifyBLECharacteristicValueChange 在 IOS 和 Andriod 上收到的内容不同？

具体表现为 IOS 调用后可以完整接收内容，Andriod 只能接收到前 20 个内容，后续接收的包也只是相同的前 20个。猜测是系统和蓝牙设备对单次传输的数据大小的限制。目前预测可行的解决办法是进行分包处理。将一次的内容分几次发送分几次接收。

思考了一下可行的解决方案：

1. 压缩全部发送内容，使其一次发完所需内容。如去掉 key，只保留 value。
2. 关闭主动广播，只有上位机请求数据才发送对应的内容。例如手机先请求温湿度，然后再请求臭氧浓度。分开多次请求不同的数据，就像 AT 指令的模式一样，但不是采取 AT 固件的方式，发送 gettemp 返回温度， 发送 geto3 返回臭氧浓度等。采取定时器不断请求数据。
3. 修改 MTU 到合适的范围。不同 BLE 版本的 MTU 最高值不同，不同安卓设备表现出来的结果也不同，需要获取设备所支持的 max mtu，如果超过协议的大小就警告。为此需要弄清楚 esp32c3 的 BLE 版本和确定通信协议的最小单元。

[小程序蓝牙-监听方法接收数据丢失问题 wangzl2018-06-05](https://developers.weixin.qq.com/community/develop/doc/0000e430aa8ca01cd7d621c055b800)

[小程序蓝牙接收监听数据丢失 chuntao 2019-04-27](https://developers.weixin.qq.com/community/develop/doc/0002ae08960c680f7978789355b800?_at=1644591707718)

[微信小程序之蓝牙 BLE 踩坑记录 HintLee1 2017.08.17](https://www.jianshu.com/p/42a8f71110e8)
> 众所周知，BLE 4.0 中发送一个数据包只能包含 20 字节的数据，大于 20 字节只能分包发送。微信小程序提供的 API 中似乎没有自动分包的功能，这就只能自己手动分包了。调试中发现，在 iOS 系统中调用 wx.writeBLECharacteristicValue 发送数据包，回调 success 后紧接着发送下一个数据包，很少出现问题，可以很快全部发送完毕。而安卓系统中，发送一个数据包成功后紧接着发送下一个，很大概率会出现发送失败的情况，在中间稍做延时再发送下一个就可以解决这个问题（不同安卓手机的时间长短也不一致），照顾下一些比较奇葩的手机，大概需要延时 250 ms 。不太好的但是比较科学的办法是，只要成功发送一个数据包则发送下一个，否则不断重发，具体就是
wx.writeBLECharacteristicValue 回调 fail 则重新发送，直至发送完毕。

[wx.writeBLECharacteristicValue(Object object)](https://developers.weixin.qq.com/miniprogram/dev/api/device/bluetooth-ble/wx.writeBLECharacteristicValue.html):onBLECharacteristicValueChange没有看到相关注意事项
> 并行调用多次会存在写失败的可能性。
  小程序不会对写入数据包大小做限制，但系统与蓝牙设备会限制蓝牙 4.0 单次传输的数据大小，超过最大字节数后会发生写入错误，建议每次写入不超过 20 字节。
  若单次写入数据过长，iOS 上存在系统不会有任何回调的情况（包括错误回调）。
  安卓平台上，在调用 wx.notifyBLECharacteristicValueChange 成功后立即调用本接口，在部分机型上会发生 10008 系统错误

[小程序蓝牙wx.onBLECharacteristicValueChange监听数据包异常？逢 01-27](https://developers.weixin.qq.com/community/develop/doc/000e00871345e8978e6d682785b000)

[在BLE蓝牙中一次写入超过20字节数据包的方法和技巧 hhyyqq5800 于 2019-12-15](https://blog.csdn.net/hhyyqq/article/details/103548820):Android

[wx.setBLEMTU(Object object)](https://developers.weixin.qq.com/miniprogram/dev/api/device/bluetooth-ble/wx.setBLEMTU.html):需要提高基础库到 2.11.0，由于此功能仅针对 android 设备，因此在 ios 上可保持我现在的 wx 版本。 
