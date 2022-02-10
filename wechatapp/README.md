# 微信小程序蓝牙控制ESP32-C3
# 一、前言


## 测试设备

| 手机型号  | 系统版本  | 微信版本   | 基础库版本       |
|:---------:|:---------:|:------:|:-----------:|
| iPhone SE | IOS 12.4 | 7.0.10 | 2.10.4[526] |
|           |          |        |             |

下位机：ESP32

## 特点

- [x] [无障碍访问](https://developers.weixin.qq.com/miniprogram/dev/component/aria-component.html)
- [] [数据可视化](https://github.com/ecomfe/echarts-for-weixin)

# 二、设备核心代码

## 2.1 蓝牙控制

Arduino ESP32 BLE_uart

# 三、微信小程序核心代码

- [微信小程序蓝牙功能 独孤求赞 于 2021-01-25](https://blog.csdn.net/weixin_44989478/article/details/112845696):程序不可直接使用，程序逻辑上没问题，但是存在bug，需要稍加修改
- [微信小程序怎么实现蓝牙连接？（代码示例）青灯夜游 转载2018-11-13](https://www.php.cn/xiaochengxu-412851.html)：以此为蓝本修改上面的代码
- []()
- []()

# 四、页面设计

- 设备搜索页：显示周围的所有设备，是否已连接
- 设备状态实时状态页：展示设备当前监测数据，具备指令操作功能
- 设备历史数据记录页：展示历史数据，提供保存接口

由 Page 过渡到 Tab