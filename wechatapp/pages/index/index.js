// index.js
// 获取应用实例
const app = getApp()

Page({
  data: {
    devicesList: [],
  },

  onLoad() {
    let that = this;
    that.openBluetoothAdapter();
    /* // 测试表明不用 gps 也可以，可是文档说 ble需要 gps 授权
    wx.getSystemInfo({ // 获取系统信息，提示打开 GPS
      success(res) {
        let gps = res.locationEnabled;
        if (!gps) {
          wx.showModal({
            title: '请打开GPS定位',
            content: '不打开GPS定位，可能无法搜索到蓝牙设备',
            showCancel: false
          })
        } else {
          that.openBluetoothAdapter();
        }
      }
    })
    */
  },

  openBluetoothAdapter() {
    /* 打开蓝牙适配器 */
    let that = this;
    wx.openBluetoothAdapter({
      success(res) {
        wx.onBluetoothAdapterStateChange(function (res) {
          if (!res.available) { //蓝牙适配器是否可用
            wx.showModal({
              title: '温馨提示',
              content: '检测到蓝牙未开启，请打开蓝牙功能',
              showCancel: false
            })
          }
        })
        that.searchBlue(); //开始搜索蓝牙
      },
      fail(res) {
        console.log(res);
        wx.showToast({
          title: '请检查手机蓝牙是否打开',
          icon: 'none',
        })
      },
    })
  },


  searchBlue() {
    let that = this;
    wx.startBluetoothDevicesDiscovery({
      allowDuplicatesKey: false,
      interval: 0,
      success(res) {
        wx.showLoading({
          title: '正在搜索设备',
        });
        wx.getBluetoothDevices({
          success: function (res) {
            console.log(res);
            let devicesListArr = [];
            if (res.devices.length > 0) { //如果有蓝牙就把蓝牙信息放到渲染列表里面
              that.setData({
                devicesList: [],
              }); // 清空列表里的设备
              wx.hideLoading();
              res.devices.forEach(device => {
                if (!device.name || !device.localName) { // 剔除没有名称的“未知设备”。高峰地铁车厢中大概有四百个
                  return
                } else {
                  devicesListArr.push(device);
                }
              })
              that.setData({
                devicesList: devicesListArr,
              }); //渲染到页面中
            } else {
              wx.hideLoading();
              wx.showModal({
                title: '温馨提示',
                content: '附近好像没有蓝牙设备，请转移地点或者开启蓝牙设备后重新尝试',
                showCancel: false
              });
            }
          }
        })
      },
      fail: function (res) {
        wx.showToast({
          title: '搜索蓝牙外围设备失败,请重新初始化蓝牙!',
          icon: 'none',
        })
        that.openBluetoothAdapter(); // 尝试再次打开蓝牙
      }
    })
  }
})