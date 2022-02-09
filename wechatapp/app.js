// app.js
App({
  onLaunch: function () {
    wx.authorize({
      scope: 'scope.bluetooth', // 向用户请求 访问蓝牙 授权
    });
  },
  globalData: {}
})