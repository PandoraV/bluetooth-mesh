// app.js
App({
  onLaunch() {
    // 展示本地存储能力
    const logs = wx.getStorageSync('logs') || []
    logs.unshift(Date.now())
    wx.setStorageSync('logs', logs)


    //目前只支持低功耗蓝牙，文档上的两套api是可以结合使用的，安卓
    wx.authorize({
      scope: 'scope.bluetooth', // 向用户请求 访问蓝牙 授权
    });
  },
  globalData: {
  }
})
