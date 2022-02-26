# 微信小程序蓝牙控制ESP32-C3
# 一、前言


## 测试设备

| 手机型号  | 系统版本  | 微信版本   | 基础库版本       |
|:---------:|:---------:|:------:|:-----------:|
| iPhone SE | IOS 12.4 | 8.0.18 | 2.22.0[657] |
| 华为 mate 20 pro(UD) | 鸿蒙 2.0.0.209 | 8.0.14 |  |

下位机：ESP32

目前功能要求最高的是由 FileSystemManager.access 所依赖的基础库版本为 2.19.2，低版本不会限制使用该小程序，只是效果上可能无法达到理想的结果

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

## 设备过滤

在地铁上写代码测试的时候发现附近有几百个显示为“未知设备”，滚动了许久才滚到我的设备。因此要在代码里过滤掉，之前通过 `!device.localName || !device.name` 过滤掉了，但是今天换一个新设备测试发现依然显示有“未知设备”。

调试发现是存在 localName 而 name 为“未知设备”。在视图里是 `{{item.name || item.localName}}` 优先渲染了 name, 这次对调了一下，先渲染 localName

切勿通过 item.name=="未知设备" 来过滤设备，不然就会再现「无SIM卡」之 bug

```javascript
// 没有 localName，某些设备可能没有
{name: "未知设备", RSSI: -92, deviceId: "5BCF6A2A-B024-3B75-88E2-9EA4F4943446"}
{deviceId: "DA730933-AEFA-B2D6-4DAA-D935C5D9DB08", name: "未知设备", advertisData: ArrayBuffer(29), RSSI: -74}

// 有 localName 而 name 是未知
{deviceId: "AD6B3F3E-809E-47C8-6537-813137813BA4", advertisServiceUUIDs: Array(1), localName: "Mi Smart Band 6", name: "未知设备", advertisData: ArrayBuffer(26), …}
```

## 数据格式转换
### str2ab

`function stringToBytes(str)`（pages/blueTooth/blueTooth.js）

### json2csv

`function jsonFake2csv(jsonobj)`（pages/blueTooth/blueTooth.js）

- https://www.cnblogs.com/sunlightlee/p/10247501.html
- https://segmentfault.com/a/1190000011389463

```json
{i_num: 2, p_mls: 1000, add: 1, temp: -1, humi: -1,time:161926}
```

收到的 json 立马转成 csv 的一行数据

`1000,1,-1,-1,,,,,161926`

## 数据存储

> 快满了自动删除旧的数据待开发待测试

最初采用缓存存储的方式存储 csv 数据，后因其容量过小，更换为文件存储的方式。

- 文件名：设备ID.csv
- 例子：`${wx.env.USER_DATA_PATH}/079E42F1-FD24-7EA6-5B09-4DD6640D888C.csv`
- 内容：

```csv
p_mls,add,temp,humi,NH3,O3,NO,NO2,time
1000,1,-1,-1,,,,,161926
1000,1,-1,-1,,,,,161927
```

i-num 不必要存储

本地用户文件最多可存储 200MB，由于本地缓存文件不具备写权限，不建议使用


- [文件系统](https://developers.weixin.qq.com/miniprogram/dev/framework/ability/file-system.html)

## 数据导出

高于此版本支持一键导出 csv 文件，低于此版本的情况下建议升级版本。由于读取文件的 API 要求基础库 2.19.2 以上，因此复制导出的办法需要做两套数据存储方案才得以实现，鉴于时间成本考虑，不再支持复制导出。

shareFileMessage 基础库版本： `2.16.1`。

[「已解决」前端生成txt或者csv文件后导出（分享）？](https://developers.weixin.qq.com/community/develop/doc/000a2ef2788900f7458dd37f555c00):wx.shareFileMessage

# 四、页面设计

- 设备搜索页：显示周围的所有设备，是否已连接
- 设备状态实时状态页：展示设备当前监测数据，具备指令操作功能
- 设备历史数据记录页：展示历史数据，提供保存接口(json转csv)

# 五、与下位机的通信

## 上位机解析数据格式

数据格式为一个`Json`结构，最外层为一个大括号，内部每个键后跟冒号，再跟键值，键值末端以**逗号**结尾，最后一个键值无逗号结尾，全部键用引号括起来，键值若为包含多个子字符串的字符串数组则用**中括号**括起来，内部每个键用**引号**括起来，中间用**逗号**隔开，数字保留原始状态。字典深度为`1`，即仅有最外层一个大括号。

### 数据内容

字典内包括数据条数，当前采样时间间隔，当前从机地址，传感器具体数据等。在收到之后加入实时时间戳。

**特别说明**，组成`json`的字符串内，相应键必须严格与下列顺序一致，因为在传回的条目名称中，传感器的数量是与下表相同的顺序相叠加，分最基础的温湿度+氨气直到全部传感器。若赋予了某特定传感器但并未检测到连接，则会返回`-1`作为测量量。下面对每一条详细说明。

表2 键名与键值数据类型对应及含义简要介绍

|键名|	 键值类型|	含义|
|---:|  :---:   |---   |
i_num|	int   |	        包含数据条数
p_mls|	uint16|	传感器采样时间间隔，单位毫秒
add |	int   |     	当前传回参数从机地址
temp|	float |	        温度
humi|	float |	        湿度
NH3 |	float |	        氨气浓度
O3  |	float |	        臭氧浓度
NO  |	float |	        一氧化氮浓度
NO2 |	float |	        二氧化氮浓度


（1）包含数据条数 i_num

- 即传回信息中包含数据条目的总数，用来辅助分割条目名称的字符串段，数据类型为`int`。

（2）当前采样时间间隔 p_mls

- 采样时间间隔，数据类型为`uint16`，推荐默认采样间隔时长为`1000`毫秒。实际存储应用`long`或`unsigned long`。

（3）地址 add

- 标记当前传回数据的从机的地址位，数据类型为`int`。在早期测试中，仅有一台从机工作，此地址位留给未来多台从机组网构建蓝牙mesh用，长度为`1`个字节。

（4）其他若干数据

- 本传感器检测的数据包括**一氧化氮浓度**、**二氧化氮浓度**、**臭氧浓度**、**氨气浓度**四种气体浓度，检测**浓度取值范围**和**精度**取决于具体传感器。对于气体传感器，其检测精度均为`0.1ppm`。

    1）温湿度传感器

    - 温湿度包括`温度（temp）`和`湿度（humi）`，均为`float`数据类型，小数位数1位，量程湿度`5~95%RH`， 温度`-20~+60℃`。由于本传感器工作环境温度不会低于零度，因此`负数`读数表示传感器工作异常。

    2）臭氧

    - 包括`臭氧浓度（O3）`，量程`0~100ppm`，分辨率是`0.1ppm`。

    3）一氧化氮

    - 包括`氮氧化物浓度（NO）`，量程`0~2000ppm`，分辨率是`0.1ppm`。

    4）二氧化氮

    - 包括`氮氧化物浓度（NO2）`，量程`0~2000ppm`，分辨率是`0.1ppm`。

    5）氨气

    - 包括`氨气浓度（NH3）`，量程`0~1000ppm`，分辨率是`0.1ppm`。

#### **解析样例demo：**

```javascript
{
    "i_num":2,
    "per_mls":1000,
    "add":1,
    "temp":22.0,
    "humi":20.0
}
```

上位机接收后会在其中加入`当地时间戳（time）`，格式为`hhmmss`，如下所示:

```javascript
{
    "i_num":2,
    "p_mls":1000,
    "add":1,
    "temp":20.3,
    "humi":42,
    "time":"151536" // 15:15:36
}
```


## 上位机向下位机发信的协议

上位机向下位机发送的内容为：上位机向下位机发送指令，用以调整从传感器读数的采样间隔；

### 命令格式

指令为一个`字符串`，在下位机上存储的格式为`std::string`。

### 命令内容

首位字符为一大写字母，表明指令类型；第二个字符为数字，取值`0~9`，表明操作的传感器对象；第三个字符为地址位，是一个数字，取值`0~9`；对于调整采样间隔的命令，若第四位为`0`，则表明这是一个小数，注意不包含**小数点**；对于查询命令，仅返回长整型数的采样间隔。

- 首位字母

    - `"P"`：调整采样间隔，注意，时间单位为毫秒
    - `"T"`: 发送文本信息
    - `"Q"`: 查询命令
        - 特别的，对于查询命令，应当避免回信的时候与定期传回的传感器数据同时调用函数，引起冲突。
        - 查询命令规定长度为4位，最后一位占位无实际意义。

- 第二位数字

    - `0`:  无指定传感器
    - `1`:  氨气传感器
    - `2`:  臭氧传感器
    - `3`:  一氧化氮传感器
    - `4`:  二氧化氮传感器
    - `5`:  温湿度传感器

- 第三位

    - 地址位 当地址位为`0`时，表明是向所有下位机发送。

- 第四位
  
    若为`0`，则说明是小数，后续无**小数点**。

#### **发信demo：**

```C
"P0105"             // 将地址1的从机的采样间隔调整为0.5s
"P010500"           // 将地址1的从机的采样间隔调整为0.5s
"P012000"           // 将地址1的从机的采样间隔调整为2s
"T01HelloWorld!"    // 向地址为1的从机发送文本"HelloWorld!"
"Q010"              // 向地址为1的从机询问当前采样时间间隔
```

# 六、FAQ

## notifyBLECharacteristicValueChange 在 IOS 和 Andriod 上收到的内容不同？

具体表现为 IOS 调用后可以完整接收内容，Andriod 只能接收到前 20 个内容，后续接收的包也只是相同的前 20个。猜测是系统和蓝牙设备对单次传输的数据大小的限制。目前预测可行的解决办法是进行分包处理。将一次的内容分几次发送分几次接收。

思考了一下可行的解决方案：

1. 压缩全部发送内容，使其一次发完所需内容。如去掉 key，只保留 value。
2. 关闭主动广播，只有上位机请求数据才发送对应的内容。例如手机先请求温湿度，然后再请求臭氧浓度。分开多次请求不同的数据，就像 AT 指令的模式一样，但不是采取 AT 固件的方式，发送 gettemp 返回温度， 发送 geto3 返回臭氧浓度等。采取定时器不断请求数据。
3. 修改 MTU 到合适的范围。不同 BLE 版本的 MTU 最高值不同，不同安卓设备表现出来的结果也不同，需要获取设备所支持的 max mtu，如果超过协议的大小就警告。为此需要弄清楚 esp32c3 的 BLE 版本和确定通信协议的最小单元。

最后采取方案 1，字节码传输，需要额外解码。MTU 安卓实验未开展。

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

## 文件下载 href

- [javascript实现生成并下载txt文件 z__a 2018-09-11](https://blog.csdn.net/zhang__ao/article/details/82625606)

```js
function download(filename, text) {
  var element = document.createElement('a');
  element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
  element.setAttribute('download', filename);
  element.style.display = 'none';
  document.body.appendChild(element);
  element.click();
  document.body.removeChild(element);
}
 
 
download("hello.txt","This is the content of my file :)");
```

在 html 测试是可以下载的，但是到小程序了就不行，href 都会被微信过滤掉，看来还是要整后端

```html
<a href="data:text/plain;charset=utf-8,This%20is%20the%20content%20of%20my%20file%20%3A)" download="test.txt">down</a>
```
