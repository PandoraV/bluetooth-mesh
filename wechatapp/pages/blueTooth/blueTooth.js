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
    allData: [] // 收到的全部蓝牙消息
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
    wx.notifyBLECharacteristicValueChange({
      state: true, // 启用 notify 功能
      deviceId: this.data.deviceId,
      serviceId,
      characteristicId,
      success(res) {
        wx.onBLECharacteristicValueChange(function (res) {
          var jsonstr = ab2str(res.value);
          try {
            var jsonobj = JSON.parse(jsonstr); // 如果不是合理的格式会出错，处理
            if (jsonobj) {
              var timenow = new Date().Format("hhmmss");  // 格式见 readme
              jsonobj.time = timenow; // 加入当地时间戳
              var str = JSON.stringify(jsonobj);
              console.log(jsonobj); // 加入 time 之后的字符串，调试用
              var tmpmsg = that.data.allData
              tmpmsg.push(jsonobj)
              that.setData({
                allData: tmpmsg,
                msg: str
              });
            }
          } catch (e) {
            console.warn(e)
            console.warn("Illogical data of json: " + jsonstr)
            wx.showToast({
              title: '收到不合理的数据: ' + jsonstr,
              icon: 'none',
            })
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