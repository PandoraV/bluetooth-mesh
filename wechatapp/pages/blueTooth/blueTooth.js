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

/* 将 json 对象解析为 csv 格式的一行数据，数据顺序按照 keys 排列
@param jsonobj： json对象
@return res：csv 格式的一行数据的字符串形式*/
function jsonFake2csv(jsonobj) {
  var keys = ["time", "temp", "humi", "NH3", "O3", "NO", "NO2"] // 顺序很重要，代表csv的列
  var res = "" // csv 数据结果
  // console.log(jsonobj[keys[i]])
  for (var i = 0; i < keys.length; i++) { //遍历数组
    if (jsonobj.hasOwnProperty(keys[i])) {
      // if (keys[i] == keys[i]) {
      //   // console.log(keys[i])
      //   if (jsonobj[keys[i]] == "")
      //   {
      //     jsonobj[keys[i]] = "0.0"
      //   }
      // }
      res += jsonobj[keys[i]]
    }
    
    if (i != keys.length - 1) {
      res += ','
    } else {
      res += '\r\n'
    }
  }
  // console.info(res)
  return res
}

/* 将 json 的 key 转换为对应的中文，返回有换行、有中文的 string */
function js2key(jsonobj) {
  // 为了前台显示数据
  var keyobj = {
    "p_mls": "采样时间间隔",
    "temp": "温度",
    "humi": "湿度",
    "NH3": "氨气浓度",
    "O3": "臭氧浓度",
    "NO": "一氧化氮浓度",
    "NO2": "二氧化氮浓度",
    "time": "时间"
  }
  var res = {}
  for (var k in jsonobj) {
    if (keyobj[k]) { // 过滤 地址
      res[keyobj[k]] = jsonobj[k]
    }
  }
  // console.info(res)
  return JSON.stringify(res, null, 2)
}

const fs = wx.getFileSystemManager()

Page({

  /**
   * 页面的初始数据
   */
  data: {
    isConnected: false, // 当前是否处于连接状态
    isListen: true, // 当前是否处于监听状态
    deviceId: '',
    connectName: '',
    serviceId: "", // 服务 ID
    uuidListen: "", // 监听接收的 uuid
    uuidWrite: "", // 发送内容的uuid
    msg: "", // 最新一条消息
    maxNO: -1, // 最大一氧化氮
    maxNO2: -1, // 最大二氧化氮
    dataFilePath: "", // 收到的全部蓝牙消息的 csv 文件路径
    period_millis: 1000, // 当前采样时间间隔，默认1秒
    slave_address: '0' // 从机地址，以后可以改成列表
  },

  onLoad: function () { 
    // this.connectPreparing()
  },

  /**
   * 生命周期函数--监听页面显示
   */
   onShow: function () {
    var app = getApp()
    // 判断当前是否处于非连接状态
    if (this.data.isConnected == false)
    {
      // 当前处于非连接状态
      var deviceId = app.globalData.current_connect_deviceID
      var connectName = app.globalData.current_connect_name
      // 判断全局变量是否存有待连接设备
      if (deviceId == "") {
        // 当前无连接，刷新当前显示
        // TODO
      } else {
        // 当前待连接，调用准备连接函数
        this.connectPreparing();
      }
    }
  },

  // 连接准备工作
  connectPreparing() {
    var that = this;
    var app = getApp();

    let deviceId = app.globalData.current_connect_deviceID; //设备id
    let connectName = app.globalData.current_connect_name; //连接的设备名称
    console.log(app.globalData)
    // console.log("decive ID is ", deviceId)

    that.setData({ // 设置数据文件路径
      deviceId: deviceId,
      connectName: connectName,
      dataFilePath: `${wx.env.USER_DATA_PATH}/` + that.data.deviceId + '.csv' // 存储路径如下，实际导出文件应重新命名
    })
    // 没有对应的数据文件就创建新的，有的话就不再新建了
    // console.log("set data complete")
    fs.access({
      path: that.data.dataFilePath,
      success(res) {
        // 文件存在，将缓存移除
        console.log("cache removed")
      },
      fail(res) {
        // 文件不存在或其他错误
        console.log("no existed document founded in the path")
      },
      complete() {
        fs.writeFile({ // 不存在就创建文件，加入 csv 表头
          filePath: that.data.dataFilePath,
          data: "time,temp,humi,NH3,O3,NO,NO2\r\n", // 写入表头
          encoding: 'utf8',
          success(res) {
            console.log(res)
          },
          fail(res) { // 创建文件失败
            console.error(res)
            wx.showToast({
              title: '创建存储文件失败，请向管理员反馈',
              icon: 'none'
            })
          }
        })
      }
    })
    // console.log("file prepared completed")
    this.connectTo(deviceId, connectName);
    // console.log("connecting started")
    // 设置连接状态
    this.setData({
      isConnected: true
    });
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
        } else if (res.errCode == 10012) {
          wx.showToast({
            title: '连接超时，请重试！',
          })
        }
      },
      fail(error) {
        // wx.hideLoading();
        wx.showToast({
          title: "看后台",
        })
        console.error(error.errCode, " the device id is ", deviceId)
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

  /* 根据 uuid 订阅蓝牙消息 */
  notifyBLECharacteristicValueChange(serviceId, characteristicId) {
    let that = this;
    wx.notifyBLECharacteristicValueChange({
      state: true, // 启用 notify 功能
      deviceId: this.data.deviceId,
      serviceId,
      characteristicId,
      success(res) {
        wx.onBLECharacteristicValueChange(function (res) {
          that.msgHandle(res.value)
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
        wx.switchTab({ // 返回设备列表页面
          url: '../index/index',
        })
        // 清空全局变量
        var app = getApp();
        app.globalData.current_connect_name = ""
        app.globalData.current_connect_deviceID = ""
        // 清空页面缓存数据
        that.setData({
          isConnected: false, // 清空连接状态
          msg: "", // 清空页面显示文字
          maxNO: -1, // 清空存储的氮氧化物最大值
          maxno2: -1
        });
      },
      fail() {
        wx.showToast({
          title: "无设备连接"
        })
        wx.switchTab({ // 返回设备列表页面
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


  /** 向蓝牙设备发送数据 
   * @param str 要发送的数据，字符串格式
   */
  sendMsg2BLE(str) {
    var buffer = stringToBytes(str)
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

  },

  /* 发送按钮回调函数 */
  btnSend(e) {
    if (e.detail.value.input == '') { // 空数据提前终止发送
      console.warn("Empty input. Stop sending!!!")
      wx.showToast({
        title: '检测不到数据，请输入内容后再次发送',
        icon: 'none',
      })
    } else {
      this.sendMsg2BLE(e.detail.value.input)
    }
  },

  // 调整时间间隔的回调函数
  timeDurationSend(e) {
    // TODO
    if (e.detail.value.input == '') {
      wx.showToast({
        title: '检测不到数据，请输入内容后再次发送',
        icon: 'none',
      })
    } else {
      // 将输入转为数字
      var inputNum = 0
      try {
        inputNum = Number(e.detail.value.input)
      } catch(e) {
        wx.showToast({
          title: '数字格式有问题',
          icon: 'none',
        })
      }
      if  (inputNum != 0) { // 为0则表示上一步出了问题
        // 判断是否合法
        if (inputNum >= 500 && inputNum <= 65535)
        {
          // 发送
          var sendCommand = "P0"
          sendCommand += this.data.slave_address
          if (inputNum < 1000)
            sendCommand += '0'
          sendCommand += inputNum
          console.log("new duration has been switched into " + inputNum)
          this.sendMsg2BLE(sendCommand)
          wx.showToast({
            title: '时间间隔已经调整为' + inputNum + "毫秒",
            icon: 'none',
          })
        } else {
          // 数字不合法
          wx.showToast({
            title: '大小不满足500到65535的范围',
            icon: 'none',
          })
        }
      }
    }
  },

  /* 处理接收到的数据，对数据进行解码 */
  msgHandle(msg) {
    let that = this;
    // 将接受到的数据转成字节数组
    var bytes_received = new Uint8Array(msg);
    var received_length = bytes_received.length;
    var i_num = 0;
    i_num += bytes_received[0] - 48;
    var address = 0;
    address += bytes_received[1] - 48;
    that.data.slave_address = address
    var period_millis = 0;
    var humidity = 0.0;
    var temperature = 0.0;
    var gas_precision = 0.0;
    var keys = ["i_num", "add", "p_mls", "temp", "humi", "NH3", "O3", "NO", "NO2", "time"]
    var jsonstr = "{\"";
    jsonstr += keys[1];
    jsonstr += "\":";
    jsonstr += address;
    // 时间间隔
    if (i_num > 1)
      period_millis = bytes_received[2] * 256 + bytes_received[3];
    jsonstr += ",\"";
    jsonstr += keys[2];
    jsonstr += "\":";
    jsonstr += String(period_millis);
    if (i_num == 1) {
      // 是传回的时间间隔
      period_millis = 0;
      for (var i = 2; i < received_length; i++) {
        period_millis *= 10;
        period_millis += bytes_received[i] - 48;
      }
      that.data.period_millis = period_millis;
      console.log("the plms is " + period_millis);
      wx.showToast({
        title: "当前时间间隔为" + period_millis,
        icon: 'none'
      })
    } else {
      if (i_num >= 2) {
        // 只有温湿度传感器
        if (bytes_received[4] == 0 && bytes_received[5] == 0) {
          // 温湿度传感器工作异常
          humidity = -1;
          temperature = -1;
        } else {
          temperature += bytes_received[4] * 1.0 + bytes_received[5] * 0.1;
          humidity += bytes_received[6] * 1.0 + bytes_received[7] * 0.1;
          // console.log("temp:"+temperature);
          // console.log(String(temperature));
        }
        // 将数字转成字符串
        jsonstr += ",";
        jsonstr += "\"";
        jsonstr += keys[3];
        jsonstr += "\":";
        jsonstr += String(temperature);
        jsonstr += ",";
        jsonstr += "\"";
        jsonstr += keys[4];
        jsonstr += "\":";
        jsonstr += String(humidity);

        // console.log(jsonstr);
        // 还包括气体传感器
        for (var i = 3; i <= i_num; i++) {
          gas_precision = 0; // 清空
          var index = 2 * (i + 1);
          if (bytes_received[index] == 255) { // 传感器工作异常
            gas_precision = -1;
          } else {
            gas_precision += bytes_received[index] * 25.6 + bytes_received[index + 1]*0.1; // 高字节
            // index++;
            // gas_precision += bytes_received[index + 1]*0.1; // 低字节
            // gas_precision *= 0.1;
            gas_precision.toFixed(1);
          }
          if (i == 5) {
            // 一氧化氮
            if (that.data.maxNO < gas_precision) {
              // 更新值
              that.setData({
                maxNO: gas_precision
              })
            }
          }
          if (i == 6) {
            // 二氧化氮
            if (that.data.maxNO2 < gas_precision) {
              // 更新值
              that.setData({
                maxNO2: gas_precision
              })
            }
          }
          // 对于
          jsonstr += ",";
          jsonstr += "\"";
          jsonstr += keys[i + 2];
          jsonstr += "\":";
          if (gas_precision == 0) {
            jsonstr += "0.0"
            // console.log(jsonstr)
          } else  {
            jsonstr += String(gas_precision);
          }
        }
      }
    }
    jsonstr += "}";
    // console.log(jsonstr)

    if (i_num > 1) { // 对于更新时间间隔，不调用json解析
      try {
        var jsonobj = JSON.parse(jsonstr); // 如果不是合理的格式会出错，处理
        // console.log(jsonobj)
        if (jsonobj) {
          // jsonobj.p_mls = that.data.period_millis // 更新为全局变量的数值
          that.data.period_millis = period_millis;
          var timenow = new Date().Format("hhmmss"); // 格式见 readme
          jsonobj.time = timenow; // 加入当地时间戳

          that.setData({ // 更新页面上的最新数据
            msg: js2key(jsonobj)
          });

          // 向数据文件末尾添加新 csv 数据
          fs.appendFile({
            filePath: that.data.dataFilePath,
            data: jsonFake2csv(jsonobj),
            encoding: 'utf8',
            success(res) {
              // console.log(res)
            },
            fail(res) {
              console.error(res)
              wx.showToast({
                title: '添加数据记录失败，请向管理员反馈',
                icon: 'none'
              })
            }
          })
        }
      } catch (e) {
        console.warn(e)
        console.log(bytes_received);
        console.warn("Illegal data of json: " + jsonstr)
        wx.showToast({
          title: '发生丢包: ' + jsonstr,
          icon: 'none',
        })
      }
    }

  },

  /* 导出 csv 数据文件，如果微信版本过低，就提示升级 */
  exportData() {
    if (wx.shareFileMessage) { // 可直接导出 csv,测试通过      
      var timenow = new Date().Format("MMM-dd_hh_mm"); // 格式见 readme
      wx.shareFileMessage({
        filePath: this.data.dataFilePath,
        fileName: this.data.connectName + '_' + timenow + '.csv', // 导出文件名应包括设备名称和日期，日期格式为名称+日期+时刻
        success() {},
        fail: console.error,
      })
    } else {
      // 待进一步测试，初步测试表明在data.js无法读取到该文件
      wx.showModal({
        title: '温馨提示',
        content: '当前微信版本过低，无法导出为文件，请升级后再使用',
        showCancel: false
      })
    }
  },

  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {},


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