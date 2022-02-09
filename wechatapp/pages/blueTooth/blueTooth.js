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

Page({

  /**
   * 页面的初始数据
   */
  data: {
    isHideList: false, //是否隐藏蓝牙列表
    isHideConnect: false, //是否隐藏连接模块
    deviceId: '',
    connectName: '',
    writeNews: {}, //写数据三个id
    msg: "None" // 收到的蓝牙消息
  },

  onLoad: function (option) {
    let deviceId = option.id; //设备id
    let connectName = option.name; //连接的设备名称
    this.connectTo(deviceId, connectName);

  },


  //开始连接，获取deviceId
  connectTo(deviceId, connectName) {
    let that = this;
    wx.showLoading({
      title: '连接中...',
    });
    wx.createBLEConnection({
      deviceId: deviceId, // 这里的 deviceId 需要已经通过 createBLEConnection 与对应设备建立链接
      success(res) {
        wx.hideLoading();
        that.stopBluetoothDevicesDiscovery(); //停止搜索蓝牙
        console.log(res);
        if (res.errCode == 0) {
          that.setData({
            deviceId: deviceId,
            connectName: connectName,
            isHideConnect: false,
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
            that.notifyBLECharacteristicValueChange(serviceId, model.uuid);
          }
          if (model.properties.write == true) { // 读写的uuid
            let characteristicId = model.uuid;
            let writeNews = {
              deviceId,
              serviceId,
              characteristicId
            };
            that.setData({
              writeNews: writeNews
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


  writeBLECharacteristicValue() { // 向蓝牙设备发送数据
    let that = this;
    var buffer = stringToBytes("Message from kearney!")
    wx.writeBLECharacteristicValue({
      deviceId: that.data.writeNews.deviceId,
      serviceId: that.data.writeNews.serviceId,
      characteristicId: that.data.writeNews.characteristicId,
      value: buffer,
      success: (res) => {
        console.log(res);
      },
      fail: (err) => {
        console.log(err)
      }
    })
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
          let str = that.ab2hex(res.value);
          console.log(str);
          that.setData({
            msg: str
          });
        })
      }
    })
  },



  //断开连接
  closeBLEConnection() {
    let that = this;
    console.log(that.data)
    wx.closeBLEConnection({
      deviceId: this.data.deviceId,
      success() {
        that.setData({
          deviceId: '',
          connectName: '',
          isHideConnect: true,
          msg: '',
        });
        wx.navigateTo({ // 返回设备列表页面
          url: '../index/index',
        })
      }
    })
  },


  // ArrayBuffer转16进制字符串
  ab2hex(buffer) {
    var hexArr = Array.prototype.map.call(
      new Uint8Array(buffer),
      function (bit) {
        return ('00' + bit.toString(16)).slice(-2)
      }
    )
    return hexArr.join('');
  },

  stopBluetoothDevicesDiscovery() {
    wx.stopBluetoothDevicesDiscovery({
      success(res) {
        console.log(res)
      }
    })
  },


  //关闭蓝牙模块
  closeBluetoothAdapter() {
    let that = this;
    wx.closeBluetoothAdapter({
      success(res) {
        that.setData({
          devicesList: [],
          isHideList: true, //是否隐藏蓝牙列表
          isHideConnect: true, //是否隐藏连接模块
        })
      }
    })
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