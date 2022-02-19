// index.js
// 获取应用实例
const app = getApp()

Page({
  data: {
    devicesList: [],
  },

  onLoad() {
    wx.getSetting({ // 查询是否授权，避免反复授权
      success(res) {
        if (!res.authSetting['scope.bluetooth']) { // 未授权
          wx.authorize({
            scope: 'scope.bluetooth', // 向用户请求 访问蓝牙 授权
          })
        }
      }
    })
    this.openBluetoothAdapter();
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
                if (!device.localName || !device.name) { // 剔除掉没有 localName 或者没有 name 的设备
                  return
                } else {
                  devicesListArr.push(device);
                }
              })
              that.setData({
                devicesList: devicesListArr,
              }); //渲染到页面中
              that.stopBluetoothDevicesDiscovery(); //停止搜索蓝牙
            } else {
              wx.hideLoading();
              wx.showModal({
                title: '温馨提示',
                content: '暂时无法搜索到蓝牙设备，请稍后重新尝试',
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
  },

  stopBluetoothDevicesDiscovery() {
    wx.stopBluetoothDevicesDiscovery({
      success(res) {
        console.log(res)
      }
    })
  }
})