Page({
  data: {
    deviceId: '',
    connectName: '',
    csvColumn: "p_mls,add,temp,humi,NH3,O3,NO,NO2,time\r\n", // csv 表头
    allData: "" // csv 数据
  },

  onLoad: function (option) {
    this.setData({
      deviceId: option.id,
      connectName: option.name
    })
    var that = this;
    wx.getStorage({ // 从缓存中读取历史数据
      key: this.data.deviceId,
      success(res) {
        // console.log(res.data)
        that.setData({
          allData: res.data,
        })
      },
      fail(e) {
        console.log(e)
      }
    })
  },

})