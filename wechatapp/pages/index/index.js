// index.js
// 获取应用实例
const app = getApp()

Page({
  data: {
    motto: 'Hello World',
    
  },
  // 事件处理函数
  bindViewTap() {
    wx.navigateTo({
      url: '../logs/logs'
    })
  },
  onLoad() {

  },


  gotoPage: function (options) {   
  //   wx.switchTab({      
  //       url: '../blueTooth/blueTooth',    //要跳转到的页面路径
  // })  
  wx.navigateTo({
    url: '../blueTooth/blueTooth',//要跳转到的页面路径
}) 
  },
})

