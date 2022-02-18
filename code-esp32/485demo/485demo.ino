#include <WiFi.h>          // Use this for WiFi instead of Ethernet.h
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <stdio.h>

HardwareSerial mySerial1(1); //软串口，用来与传感器进行通信
unsigned char item[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x03, 0x05, 0xCB}; 

IPAddress server_addr(***,***,**,**);  //mysql数据库的ip地址e
char user[] = "*****";              // 数据库的用户名
char password[] = "********";        // 数据库的登陆密码

// Sample query
char INSERT_SQL[] = "INSERT INTO grtrace.arduino_test( tem, hem) VALUES ('%s','%s')";
//grtrace.arduino_test( tem, hem)  建立的数据库名称.表名（值，值）

// WiFi card example
char ssid[] = "*****";         // ESP32连接的无线名称
char pass[] = "**********";     // ESP32连接的无线密码

WiFiClient client;            // Use this for WiFi instead EthernetClient
MySQL_Connection conn(&client);
MySQL_Cursor* cursor;
//下面定义了一个函数，用来与传感器通信和发送温湿度的值到数据库
double *readAndRecordData() {
  static double linshi_d[2];
  String tem="";
  String hem_t ="";
  char tem1[5];
  char hem[4]; 
  String data = ""; 
  char buff[128];// 定义存储传感器数据的数组
  String info[11];
  for (int i = 0 ; i < 8; i++) {  // 发送测温命令
   mySerial1.write(item[i]);   // write输出
  }
  delay(100);  // 等待测温数据返回
  data = "";
  while (mySerial1.available()) {//从串口中读取数据
    unsigned char in = (unsigned char)mySerial1.read();  // read读取
    Serial.print(in, HEX);
    Serial.print(',');
    data += in;
    data += ',';
  }
  if (data.length() > 0) { //先输出一下接收到的数据
    Serial.print(data.length());
    Serial.println();
    Serial.println(data);
    int commaPosition = -1;
    // 用字符串数组存储
    for (int i = 0; i < 11; i++) {
      commaPosition = data.indexOf(',');
      if (commaPosition != -1)
      {
        info[i] = data.substring(0, commaPosition);
        data = data.substring(commaPosition + 1, data.length());
      }
      else {
        if (data.length() > 0) {  
          info[i] = data.substring(0, commaPosition);
        }
      }
    }
  }
  tem = dtostrf((info[3].toInt() * 256 + info[4].toInt())/10.0,2,1,tem1);
  Serial.print("tem:");
  Serial.println(tem);
  hem_t = dtostrf((info[5].toInt() * 256 + info[6].toInt())/10.0,2,1,hem);
  dtostrf((info[5].toInt() * 256 + info[6].toInt())/10.0,2,1,hem);
  Serial.print("hem:");
  Serial.println(hem);
  sprintf(buff,INSERT_SQL, tem ,hem); 
  Serial.println(buff);
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn); // 创建一个Mysql实例
  cur_mem->execute(buff);         // 将采集到的温湿度值插入数据库中
  Serial.println("读取传感器数据，并写入数据库");
  delete cur_mem;        // 删除mysql实例为下次采集作准备
  //这里我在项目的应用中，需要把温湿度的数据提取出来，在其他的地方使用，如果实现数据上传功能，下面三行程序可不需要。
  linshi_d[0] =(info[3].toInt() * 256 + info[4].toInt())/10.0 ;
  linshi_d[1] =(info[5].toInt() * 256 + info[6].toInt())/10.0;
  return linshi_d;
}

void setup()
{
  Serial.begin(4800);
  mySerial1.begin(4800,SERIAL_8N1,35,12);
  while (!Serial); // wait for serial port to connect. Needed for Leonardo only
  
  // Begin WiFi section
  Serial.printf("\nConnecting to %s", ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // print out info about the connection:
  Serial.println("\nConnected to network");
  Serial.print("My IP address is: ");
  Serial.println(WiFi.localIP());

  Serial.print("Connecting to SQL...  ");
  if (conn.connect(server_addr, 3366, user, password))
    Serial.println("OK.");
  else
    Serial.println("FAILED.");
  
  // create MySQL cursor object
  cursor = new MySQL_Cursor(&conn);
}

void loop()
{
  if (conn.connected()) {
    double *linshi_tem;
    double *linshi_hem;
    double *linshi_temp;
    //调用readAndRecordData()即可
    linshi_temp = readAndRecordData();
    *linshi_tem = *(readAndRecordData()+1); 
    Serial.println(*linshi_temp);
    Serial.println(*linshi_tem);   
    delay(5000);
  } 
}
