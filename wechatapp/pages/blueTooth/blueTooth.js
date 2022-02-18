// pages/blueTooth/blueTooth.js

// 字符串转byte
function stringToBytes(str) {
  var array = new Uint8Array(str.length);
  for (var i = 0, l = str.length; i < l; i++) {
    array[i] = str.charCodeAt(i);
  }
  console.log(array);
  return array.buffer;
}

/** ArrayBuffer转 utf8 字符串  */
function ab2str(buf) {
  let encodedString = String.fromCodePoint.apply(null, new Uint8Array(buf));
  let decodedString = decodeURIComponent(escape(encodedString)); //没有这一步中文会乱码
  // console.log(decodedString); // 输出转换结果，调试用
  return decodedString
}

/* 
  对Date的扩展，将 Date 转化为指定格式的String
  月(M)、日(d)、小时(h)、分(m)、秒(s)、季度(q) 可以用 1-2 个占位符，
  年(y)可以用 1-4 个占位符，毫秒(S)只能用 1 个占位符(是 1-3 位的数字)
  例子：
  (new Date()).Format("yyyy-MM-dd hh:mm:ss.S") ==> 2006-07-02 08:09:04.423
  (new Date()).Format("yyyy-M-d h:m:s.S") ==> 2006-7-2 8:9:4.18
  refer:https://blog.csdn.net/yuezhuo_752/article/details/86580998
 */
Date.prototype.Format = function (fmt) {
  var o = {
    "M+": this.getMonth() + 1, // 月份
    "d+": this.getDate(), // 日
    "h+": this.getHours(), // 小时
    "m+": this.getMinutes(), // 分
    "s+": this.getSeconds(), // 秒
    "q+": Math.floor((this.getMonth() + 3) / 3), // 季度
    "S": this.getMilliseconds() // 毫秒
  };
  if (/(y+)/.test(fmt))
    fmt = fmt.replace(RegExp.$1, (this.getFullYear() + "").substr(4 - RegExp.$1.length));
  for (var k in o)
    if (new RegExp("(" + k + ")").test(fmt)) fmt = fmt.replace(RegExp.$1, (RegExp.$1.length == 1) ? (o[k]) : (("00" + o[k]).substr(("" + o[k]).length)));
  return fmt;
}

/* 将 json 对象解析为 csv 格式的一行数据，数据顺序按照  keys 排列
@param jsonobj： json对象
@return res：csv 格式的一行数据的字符串形式*/
function jsonFake2csv(jsonobj) {
  var keys = ["p_mls", "add", "temp", "humi", "NH3", "O3", "NO", "NO2", "time"] // 顺序很重要，代表csv的列
  var res = "" // csv 数据结果
  for (var i = 0; i < keys.length; i++) { //遍历数组
    if (jsonobj[keys[i]]) {
      // if(keys[i]!="time"){  // 用 time 判断是不是最后一列
      if ((i + 1) != keys.length) { // 用 length 判断是不是最后一列
        res = res + jsonobj[keys[i]] + ','
      } else { // 最后一列
        res = res + jsonobj[keys[i]] + '\r\n' // 添加三种系统的换行符号
      }
    } else { // json 中没有对于的数据
      res = res + ','
    }
  }
  // console.info(res)
  return res
}

/* 将 json 的 key 转换为对应的中文 */
function js2key(jsonobj) {
  var keyobj = {
    "i_num": "数据条数",
    "p_mls": "采样时间间隔",
    "add": "从机地址",
    "temp": "温度",
    "humi": "湿度",
    "NH3": "氨气浓度",
    "O3": "臭氧浓度",
    "NO": "氮氧化物浓度",
    "NO2": "氮氧化物浓度",
    "time": "时间"
  }
  var res = {}
  for (var k in jsonobj) {
    res[keyobj[k]] = jsonobj[k]
  }
  // console.info(res)
  return JSON.stringify(res)
}


Page({

  /**
   * 页面的初始数据
   */
  data: {
    isListen: true,
    deviceId: '',
    connectName: '',
    serviceId: "", // 服务 ID
    uuidListen: "", // 监听接收的 uuid
    uuidWrite: "", // 发送内容的uuid
    msg: "", // 最新一条消息
    allData: "", // 收到的全部蓝牙消息
    period_millis: 1000 // 当前采样时间间隔，默认1秒
  },

  onLoad: function (option) {
    let deviceId = option.id; //设备id
    let connectName = option.name; //连接的设备名称
    var that = this;
    wx.getStorage({ // 从缓存中读取历史数据
      key: deviceId,
      success(res) {
        console.log(res.data)
        that.setData({
          allData: res.data,
        })
      },
      fail(e) {
        console.log(e)
      }
    })
    this.connectTo(deviceId, connectName);

  },


  //开始连接，获取deviceId
  connectTo(deviceId, connectName) {
    let that = this;
    // wx.showLoading({
    //   title: '连接中...',
    // });
    wx.createBLEConnection({
      deviceId: deviceId, // 这里的 deviceId 需要已经通过 createBLEConnection 与对应设备建立链接
      success(res) {
        // wx.hideLoading();
        console.log(res);
        if (res.errCode == 0) {
          that.setData({
            deviceId: deviceId,
            connectName: connectName,
          })
          that.getBLEDeviceServices(deviceId); //获取已连接蓝牙的服务
          // 获取当前设备系统种类
          var isApple = true;
          wx.getSystemInfo({
            success: (result) => {
              var current_system = result.platform;
              if (current_system[0] == 'i') {
                isApple = true;
              } else {
                isApple = false;
              }
              // 向下位机发送身份标识
              var buffer = "";
              if (isApple) {
                buffer = "T00AP";
              } else {
                buffer = "T00AN";
              }
              var buffer_array = new ArrayBuffer(5);
              var buffer_view = new Uint8Array(buffer_array);
              for (var i = 0; i < 5; i++) {
                buffer_view[i] = buffer.charCodeAt(i);
              }
              console.log("array:" + buffer_array.value);
              console.log("view:" + buffer_view);
              setTimeout(() => {
                wx.writeBLECharacteristicValue({
                  deviceId: that.data.deviceId,
                  serviceId: that.data.serviceId,
                  characteristicId: that.data.uuidWrite,
                  value: buffer_array,
                  success: (res) => {
                    console.log(res);
                  },
                  fail: (err) => {
                    console.log(err)
                  }
                })
              }, 1000);
            }
          })
          // console.log(isApple);
        } else if (res.errCode == 10012) {
          wx.showToast({
            title: '连接超时，请重试！',
          })
        }
      },
      fail(error) {
        wx.hideLoading();
        wx.showToast({
          title: error,
        })
      }
    });
  },


  /* 每个蓝牙都有数个服务，起到不同的功能，每个服务有独立的uuid  获取服务以及服务的uuid      */
  getBLEDeviceServices(deviceId) {
    let serviceId;
    wx.getBLEDeviceServices({ //获取蓝牙设备所有服务
      deviceId,
      success: (res) => { //services：设备服务列表 uuid：服务id
        console.log(res);
        serviceId = res.services[0].uuid;
        this.setData({
          serviceId: serviceId
        });
        this.getBLEDeviceCharacteristics(deviceId, serviceId);
      },
    })
  },


  /* 每个服务都有特征值，特征值也有uuid，获取特征值(是否能读写) */
  getBLEDeviceCharacteristics(deviceId, serviceId) {
    let that = this;
    wx.getBLEDeviceCharacteristics({
      deviceId,
      serviceId,
      success: (res) => {
        console.log(res);
        for (var i = 0; i < res.characteristics.length; i++) {
          var model = res.characteristics[i]
          if (model.properties.notify == true) { // 监听的uuid
            this.setData({
              uuidListen: model.uuid
            });
            that.notifyBLECharacteristicValueChange(serviceId, model.uuid);
          }
          if (model.properties.write == true) { // 读写的uuid
            that.setData({
              uuidWrite: model.uuid
            });
          }
        }
      },
      fail(err) {
        console.log('获取特征值失败:');
        console.log(err)
      },
    });

  },


  notifyBLECharacteristicValueChange(serviceId, characteristicId) {
    let that = this;
    var isApple = true;
    wx.getSystemInfo({ // 看平台 此部分代码将在后续版本中移除
      success: (result) => {
        var current_system = result.platform;
        if (current_system[0] == 'i') {
          isApple = true;
        } else {
          isApple = false;
        }
      }
    })
    // console.log(isApple);
    wx.notifyBLECharacteristicValueChange({
      state: true, // 启用 notify 功能
      deviceId: this.data.deviceId,
      serviceId,
      characteristicId,
      success(res) {
        wx.onBLECharacteristicValueChange(function (res) {
          if (isApple) // 此部分代码将在后续部分调换位置
          {
            try {
              var jsonstr = ab2str(res.value);
              var jsonobj = JSON.parse(jsonstr); // 如果不是合理的格式会出错，处理
              if (jsonobj) {
                var timenow = new Date().Format("hhmmss"); // 格式见 readme
                jsonobj.time = timenow; // 加入当地时间戳
                // console.log(jsonobj); // 加入 time 之后的字符串，调试用
                var csv = jsonFake2csv(jsonobj)
                var tmpmsg = that.data.allData
                tmpmsg = tmpmsg + csv
                // console.log(tmpmsg)
                that.setData({
                  allData: tmpmsg,
                  msg: js2key(jsonobj)
                });
              }
            } catch (e) {
              // if (jsonstr.length > 0) { // 访问数组防止溢出
              //   if (jsonstr[0] == '{') // 这一部分代码只在调试时运行，此部分代码将在后续版本中移除
              //   {
              //     if (!isApple){
              //       // ANDROID
              //       wx.showToast({
              //         title: '收到数据与平台不匹配' + jsonstr,
              //         icon: 'none',
              //       })
              //       // 向下位机发信
              //     }

              //   }
              // }
              // if (isApple)
              // {
              console.warn(e)
              console.warn("Illogical data of json: " + jsonstr)
              wx.showToast({
                title: '发生丢包: ' + jsonstr,
                icon: 'none',
              })
              // }
            }
          } else { // ANDROID
            // 将接受到的数据转成字节数组
            var bytes_received = new Uint8Array(res.value);
            // console.log(new Uint8Array(res.value));
            var received_length = bytes_received.length;
            var i_num = bytes_received[0] - '0';
            var address = bytes_received[1] - '0';
            var humidity = 0.0;
            var temperature = 0.0;
            var gas_precision = 0.0;
            var keys = ["i_num", "add", "temp", "humi", "NH3", "O3", "NO", "NO2", "time"]
            var jsonstr = "{\"";
            jsonstr += keys[1];
            jsonstr += "\":";
            jsonstr += address;
            if (i_num == 1) {
              // 是传回的时间间隔
              // TODO
            } else {
              if (i_num >= 2) {
                // 只有温湿度传感器
                if (bytes_received[2] == 0 && bytes_received[3] == 0) {
                  // 温湿度传感器工作异常
                  humidity = -1;
                  temperature = -1;
                } else {
                  temperature += bytes_received[2] * 1.0 + bytes_received[3] * 0.1;
                  humidity += bytes_received[4] * 1.0 + bytes_received[5] * 0.1;
                  // console.log("temp:"+temperature);
                  // console.log(String(temperature));
                }
                // 将数字转成字符串
                jsonstr += ",";
                jsonstr += "\"";
                jsonstr += keys[2];
                jsonstr += "\":";
                jsonstr += String(temperature);
                jsonstr += ",";
                jsonstr += "\"";
                jsonstr += keys[3];
                jsonstr += "\":";
                jsonstr += String(humidity);

                // console.log(jsonstr);
                // 还包括气体传感器
                // TODO
              }
            }
            jsonstr += "}";

            var jsonobj = JSON.parse(jsonstr); // 解析json

            var timenow = new Date().Format("hhmmss"); // 格式见 readme
            jsonobj.time = timenow; // 加入当地时间戳
            var str = JSON.stringify(jsonobj);
            // console.log(jsonobj); // 加入 time 之后的字符串，调试用
            var csv = jsonFake2csv(jsonobj)
            var tmpmsg = that.data.allData
            tmpmsg = tmpmsg + csv
            // console.log(tmpmsg)
            that.setData({
              allData: tmpmsg,
              msg: str
            });
          }

          // 存储到缓存，最大数据长度为 1MB
          wx.setStorage({
            key: that.data.deviceId,
            data: that.data.allData,
            success(e) {},
            fail(e) {
              console.warn(e)
            }
          })
        })
      }
    })
  },


  //断开连接
  closeBLEConnection() {
    let that = this;
    wx.closeBLEConnection({
      deviceId: this.data.deviceId,
      success() {
        wx.navigateTo({ // 返回设备列表页面
          url: '../index/index',
        })
      }
    })
  },


  //关闭蓝牙模块
  closeBluetoothAdapter() {
    let that = this;
    wx.closeBluetoothAdapter({
      success(res) {
        console.log(res)
      }
    })
  },

  /** 是否监听设备 */
  checkBoxChange(e) {
    if (e.detail.value == '') { // 关闭监听
      this.setData({
        isListen: false
      });
      wx.offBLECharacteristicValueChange()
    } else { // 开始监听 
      this.setData({
        isListen: true
      });
      this.notifyBLECharacteristicValueChange(this.data.serviceId, this.data.uuidListen)
    }
    console.info("listen: ", this.data.isListen)
  },


  /**  向蓝牙设备发送数据 */
  sendInput(e) {
    if (e.detail.value.input == '') { // 空数据提前终止发送
      console.warn("Empty input. Stop send!!!")
      wx.showToast({
        title: '检测不到数据，请输入内容后再次发送',
        icon: 'none',
      })
    } else {
      var buffer = stringToBytes(e.detail.value.input)
      wx.writeBLECharacteristicValue({
        deviceId: this.data.deviceId,
        serviceId: this.data.serviceId,
        characteristicId: this.data.uuidWrite,
        value: buffer,
        success: (res) => {
          console.log(res);
        },
        fail: (err) => {
          console.log(err)
        }
      })
    }
  },


  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {},

  /**
   * 生命周期函数--监听页面显示
   */
  onShow: function () {},

  /**
   * 生命周期函数--监听页面隐藏
   */
  onHide: function () {},

  /**
   * 生命周期函数--监听页面卸载，向左滑动回到上一页
   */
  onUnload: function () {
    let that = this;
    that.closeBLEConnection();
  },

  /**
   * 页面相关事件处理函数--监听用户下拉动作
   */
  onPullDownRefresh: function () {},

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {},

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {}
})