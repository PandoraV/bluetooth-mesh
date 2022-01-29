# 微信小程序蓝牙控制ESP32-C3应用示范
# 一、前言

目前市场上越来越火的 Combo 方案（**Ble+WiFi**），比如平头哥的TG7100C方案、乐鑫的ESP32等，如何高效使用蓝牙和wifi通讯，已经成为了必然的趋势，于是乎，做了个这样快速入门的demo给各位，奉献于物联网；

本项目适合的模组有： 

| 模组                   | 链接                  |
| ---------------------- | --------------------- |
| 安信可ESP32-S模组      | http://yli08.cn/wjy03 |
| 安信可ESP32-SL模组     | http://yli08.cn/IKalA |
| 安信可ESP32-C3系列模组 | 联系商务              |

本开源工程用到的技术点有：

1. 乐鑫物联网操作框架 esp-idf 的 freeRtos 实时操作系统熟悉，包括任务创建/消息队列/进程间通讯；
2. 微信小程序开发基础，包括MQTT库/低功耗蓝牙API接口使用，包括搜索/连接/通讯；
3. 使用乐鑫封装 RMT 驱动层单线驱动WS2812B，实现彩虹等效果；
4. 对ESP32/C3芯片的外设开发熟悉，对BLE API接口使用熟悉，包括自定义广播/名字/自定义UUID；

# 二、设备核心代码

## 2.1 蓝牙控制

设置蓝牙广播名字

```c
esp_ble_gap_set_device_name(TEST_DEVICE_NAME);
```

设置服务UUID

```c
 gl_profile_tab[0].service_id.is_primary = true;
 gl_profile_tab[0].service_id.id.inst_id = 0x00;
 gl_profile_tab[0].service_id.id.uuid.len = ESP_UUID_LEN_16;
 gl_profile_tab[0].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;
```

主动通知上位机数据发生改动：

```c
 case ESP_GATTS_READ_EVT:
    {
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 3;
        rsp.attr_value.value[0] = red;
        rsp.attr_value.value[1] = green;
        rsp.attr_value.value[2] = blue;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        break;
    }
```

上位机主动发送数据到此并做出对应的处理：

```c
 case ESP_GATTS_WRITE_EVT:
    {
        if (!param->write.is_prep)
        {
            ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
            esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
            //发送数据到队列
            struct __User_data *pTmper;
            sprintf(user_data.allData, "{\"red\":%d,\"green\":%d,\"blue\":%d}", param->write.value[0], param->write.value[1], param->write.value[2]);
            pTmper = &user_data;
            user_data.dataLen = strlen(user_data.allData);
            xQueueSend(ParseJSONQueueHandler, (void *)&pTmper, portMAX_DELAY);

            ESP_LOGI(GATTS_TAG, "%02x %02x %02x ", param->write.value[0], param->write.value[1], param->write.value[2]);
        }
        example_write_event_env(gatts_if, &a_prepare_write_env, param);
        break;
    }
```


## 三、微信小程序核心代码

代码架构，UI主要采用第三方库：有 WeUI、Vant-UI库，其中的MQTT库采用开源的MQTT.JS库。

<p align="center">
  <img src="images/mini.jpg"  alt="Banner"   />
</p>

## 3.1 蓝牙搜索

```
 wx.onBluetoothDeviceFound(function (devices) {
      var isnotexist = true
      if (devices.deviceId) {
        if (devices.advertisData) {
          devices.advertisData = app.buf2hex(devices.advertisData)
        } else {
          devices.advertisData = ''
        }
        for (var i = 0; i < that.data.devicesList.length; i++) {
          if (devices.deviceId == that.data.devicesList[i].deviceId) {
            isnotexist = false
          }
        }
        if (isnotexist && devices[0].name === that.data.filterName ) {
          that.data.devicesList.push(devices[0])
        }
      }
      that.setData({
        devicesList: that.data.devicesList
      })
    })
  }
```

## 3.2 蓝牙服务发现

发现服务列表：```wx.getBLEDeviceServices()```

发现特征值列表：```wx.getBLEDeviceCharacteristics()```

发送设备，判断是否为蓝牙控制或wifi控制：

```javascript
 SendTap: function (red, green, blue) {
    var that = this
    if (!this.data.isCheckOutControl) {
      if (this.data.connected) {
        var buffer = new ArrayBuffer(that.data.inputText.length)
        var dataView = new Uint8Array(buffer)
        dataView[0] = red; 
        dataView[1] = green; 
        dataView[2] = blue;
        wx.writeBLECharacteristicValue({
          deviceId: that.data.connectedDeviceId,
          serviceId: that.data.serviceId,
          characteristicId: "0000FF01-0000-1000-8000-00805F9B34FB",
          value: buffer,
          success: function (res) {
            console.log('发送成功')
          }, fail() {
            wx.showModal({
              title: '提示',
              content: '蓝牙已断开',
              showCancel: false,
              success: function (res) {
              }
            })
          }
        })
      } else {
        wx.showModal({
          title: '提示',
          content: '蓝牙已断开',
          showCancel: false,
          success: function (res) {
            that.setData({
              searching: false
            })
          }
        })
      }
    } else {
      //MQTT通讯发送
      if (this.data.client && this.data.client.connected) {
        this.data.client.publish('/esp32-c3/7cdfa1322e68/devSub', JSON.stringify({red,green,blue}));
      } else {
        wx.showToast({
          title: '请先连接服务器',
          icon: 'none',
          duration: 2000
        })
      }
    }
  },
```


# 四、感谢

为此，还开源了以下的代码仓库，共勉！！

| 开源项目                                                  | 地址                                                        | 开源时间 |
| --------------------------------------------------------- | ----------------------------------------------------------- | -------- |
| 微信小程序连接mqtt服务器，控制esp8266智能硬件             | https://github.com/xuhongv/WeChatMiniEsp8266                | 2018.11  |
| 微信公众号airkiss配网以及近场发现在esp8266 rtos3.1 的实现 | https://github.com/xuhongv/xLibEsp8266Rtos3.1AirKiss        | 2019.3   |
| 微信公众号airkiss配网以及近场发现在esp32 esp-idf 的实现   | https://github.com/xuhongv/xLibEsp32IdfAirKiss              | 2019.9   |
| 微信小程序控制esp8266实现七彩效果项目源码                 | https://github.com/xuhongv/WCMiniColorSetForEsp8266         | 2019.9   |
| 微信小程序蓝牙配网blufi实现在esp32源码                    | https://github.com/xuhongv/BlufiEsp32WeChat                 | 2019.11  |
| 微信小程序蓝牙ble控制esp32七彩灯效果                      | https://blog.csdn.net/xh870189248/article/details/101849759 | 2019.10  |
| 可商用的事件分发的微信小程序mqtt断线重连框架              | https://blog.csdn.net/xh870189248/article/details/88718302  | 2019.2   |
| 微信小程序以 websocket 连接阿里云IOT物联网平台mqtt服务器  | https://blog.csdn.net/xh870189248/article/details/91490697  | 2019.6   |
| 微信公众号网页实现连接mqtt服务器                          | https://blog.csdn.net/xh870189248/article/details/100738444 | 2019.9   |
| 自主开发微信小程序接入腾讯物联开发平台，实现一键配网+控制 | https://github.com/xuhongv/AiThinkerIoTMini                 | 2020.9   |
| 云云对接方案天猫精灵小爱同学服务器+嵌入式Code开源         | https://github.com/xuhongv/xClouds-php                      | 2020.7   |

另外，感谢：

- 乐鑫物联网操作系统：https://github.com/espressif/esp-idf
- 腾讯WeUI框架：https://github.com/Tencent/weui-wxss
- 有赞Vant框架：https://vant-contrib.gitee.io/vant-weapp