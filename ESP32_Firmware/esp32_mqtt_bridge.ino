/**
 * ESP32-C3 MQTT Bridge Firmware
 * 
 * 功能：
 * 1. 自动连接 WiFi
 * 2. 自动连接 MQTT Broker
 * 3. 接收 STM32 二进制数据帧
 * 4. 解析数据并打包成 JSON
 * 5. 发送到 MQTT Broker
 * 
 * 硬件连接：
 * - UART0 (GPIO20/21): 调试输出 (USB)
 * - UART1 (GPIO6/7):   连接 STM32
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ==================== 配置 ====================
// WiFi 配置
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// MQTT 配置
const char* MQTT_BROKER_IP = "192.168.1.100";
const int MQTT_BROKER_PORT = 1883;
const char* MQTT_CLIENT_ID = "PV_Inverter_001";
const char* MQTT_USERNAME = "inverter";
const char* MQTT_PASSWORD = "password";
const int MQTT_KEEPALIVE = 60;

// 主题定义
const char* TOPIC_DATA = "pv/devices/PV-INV-001/data";
const char* TOPIC_STATUS = "pv/devices/PV-INV-001/status";
const char* TOPIC_CONTROL = "pv/devices/PV-INV-001/control";
const char* TOPIC_EVENT = "pv/devices/PV-INV-001/event";

// UART 配置
#define STM32_UART Serial1
#define STM32_BAUDRATE 115200
#define STM32_RX_PIN 6
#define STM32_TX_PIN 7

// 数据帧配置
#define FRAME_HEAD_1 0xAA
#define FRAME_HEAD_2 0x55
#define FRAME_MIN_LEN 4
#define FRAME_MAX_LEN 256

// ==================== 全局变量 ====================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// 接收缓冲区
uint8_t rxBuffer[FRAME_MAX_LEN];
uint16_t rxIndex = 0;
bool frameStarted = false;

// 系统状态
bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastReconnectAttempt = 0;

// ==================== CRC8 计算 ====================
uint8_t CRC8_Calculate(const uint8_t* data, uint16_t len) {
  uint8_t crc = 0x00;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// ==================== WiFi 连接 ====================
void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
  } else {
    Serial.println("\nWiFi Connection Failed!");
    wifiConnected = false;
  }
}

// ==================== MQTT 连接 ====================
void connectMQTT() {
  if (!wifiConnected) return;
  
  Serial.println("Connecting to MQTT Broker...");
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("MQTT Connected!");
    mqttConnected = true;
    
    // 订阅控制主题
    mqttClient.subscribe(TOPIC_CONTROL);
    Serial.println("Subscribed to control topic");
    
    // 发送上线状态
    sendStatusMessage("ONLINE");
  } else {
    Serial.print("MQTT Connection Failed, rc=");
    Serial.println(mqttClient.state());
    mqttConnected = false;
  }
}

// ==================== 发送状态消息 ====================
void sendStatusMessage(const char* state) {
  if (!mqttConnected) return;
  
  StaticJsonDocument<256> doc;
  doc["ts"] = millis() / 1000;
  doc["state"] = state;
  doc["uptime"] = millis() / 1000;
  doc["fw_version"] = "v1.0.0";
  doc["wifi_rssi"] = WiFi.RSSI();
  
  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_STATUS, buffer);
}

// ==================== 解析二进制数据帧 (72字节) ====================
void parseDataFrame(uint8_t* data, uint16_t len) {
  if (len < 72) {
    Serial.println("Error: Data frame too short (expected 72 bytes)");
    return;
  }
  
  // 解析数据帧 (72字节)
  // [0-3] 时间戳
  uint32_t timestamp;
  memcpy(&timestamp, &data[0], 4);
  
  // [4-7] PV电压
  float pvVoltage;
  memcpy(&pvVoltage, &data[4], 4);
  
  // [8-11] PV电流
  float pvCurrent;
  memcpy(&pvCurrent, &data[8], 4);
  
  // [12-15] PV功率
  float pvPower;
  memcpy(&pvPower, &data[12], 4);
  
  // [16-19] 母线电压
  float busVoltage;
  memcpy(&busVoltage, &data[16], 4);
  
  // [20-23] 交流电压
  float acVoltage;
  memcpy(&acVoltage, &data[20], 4);
  
  // [24-27] 交流电流
  float acCurrent;
  memcpy(&acCurrent, &data[24], 4);
  
  // [28-31] 交流功率
  float acPower;
  memcpy(&acPower, &data[28], 4);
  
  // [32-35] 交流频率
  float acFrequency;
  memcpy(&acFrequency, &data[32], 4);
  
  // [36-39] 功率因数
  float acPf;
  memcpy(&acPf, &data[36], 4);
  
  // [40-43] 前级温度
  float tempFront;
  memcpy(&tempFront, &data[40], 4);
  
  // [44-47] 后级温度
  float tempRear;
  memcpy(&tempRear, &data[44], 4);
  
  // [48-51] MPPT占空比
  float mpptDuty;
  memcpy(&mpptDuty, &data[48], 4);
  
  // [52-55] MPPT效率
  float mpptEfficiency;
  memcpy(&mpptEfficiency, &data[52], 4);
  
  // [56-59] THD
  float thd;
  memcpy(&thd, &data[56], 4);
  
  // [60-63] 电网频率
  float gridFreq;
  memcpy(&gridFreq, &data[60], 4);
  
  // [64-67] 电网相位
  float gridPhase;
  memcpy(&gridPhase, &data[64], 4);
  
  // [68-71] 状态字
  uint32_t status;
  memcpy(&status, &data[68], 4);
  
  // 解析状态位
  uint8_t invState = status & 0x03;          // Bit 0-1: 逆变器状态
  uint8_t mpptState = (status >> 2) & 0x03;  // Bit 2-3: MPPT状态
  bool syncState = (status >> 4) & 0x01;     // Bit 4: 锁相状态
  bool mode = (status >> 5) & 0x01;          // Bit 5: 运行模式
  uint8_t faultCode = (status >> 8) & 0xFF;  // Bit 8-15: 故障代码
  
  // 计算转换效率
  float efficiency = (pvPower > 0) ? (acPower / pvPower * 100) : 0;
  
  // 构建 JSON
  StaticJsonDocument<1024> doc;
  
  doc["ts"] = timestamp;
  
  JsonObject pv = doc.createNestedObject("pv");
  pv["voltage"] = round(pvVoltage * 100) / 100;
  pv["current"] = round(pvCurrent * 1000) / 1000;
  pv["power"] = round(pvPower * 100) / 100;
  
  JsonObject bus = doc.createNestedObject("bus");
  bus["voltage"] = round(busVoltage * 100) / 100;
  
  JsonObject ac = doc.createNestedObject("ac");
  ac["voltage"] = round(acVoltage * 100) / 100;
  ac["current"] = round(acCurrent * 1000) / 1000;
  ac["power"] = round(acPower * 100) / 100;
  ac["frequency"] = round(acFrequency * 100) / 100;
  ac["pf"] = round(acPf * 100) / 100;
  
  JsonObject temp = doc.createNestedObject("temp");
  temp["mosfet_front"] = round(tempFront * 10) / 10;
  temp["mosfet_rear"] = round(tempRear * 10) / 10;
  
  JsonObject mppt = doc.createNestedObject("mppt");
  // MPPT状态: 0=SCANNING, 1=TRACKING, 2=LOCKED
  const char* mpptStateStr[] = {"SCANNING", "TRACKING", "LOCKED", "UNKNOWN"};
  mppt["state"] = mpptStateStr[mpptState < 3 ? mpptState : 3];
  mppt["duty"] = (uint8_t)mpptDuty;
  mppt["efficiency"] = round(mpptEfficiency * 10) / 10;
  
  JsonObject inverter = doc.createNestedObject("inverter");
  // 逆变器状态: 0=STOP, 1=RUN, 2=FAULT
  const char* invStateStr[] = {"STOP", "RUN", "FAULT", "UNKNOWN"};
  inverter["state"] = invStateStr[invState < 3 ? invState : 3];
  inverter["mode"] = mode ? "OFF_GRID" : "ON_GRID";
  inverter["syncState"] = syncState ? "SYNCED" : "UNSYNC";
  inverter["thd"] = round(thd * 100) / 100;
  inverter["gridFreq"] = round(gridFreq * 100) / 100;
  inverter["gridPhase"] = round(gridPhase * 10) / 10;
  inverter["faultCode"] = faultCode;
  inverter["faultDesc"] = faultCode > 0 ? getFaultString(faultCode) : "";
  
  // 序列化并发送
  char buffer[1024];
  size_t n = serializeJson(doc, buffer);
  
  if (mqttConnected) {
    mqttClient.publish(TOPIC_DATA, buffer);
    Serial.println("Data published to MQTT");
  }
  
  // 调试输出
  Serial.println(buffer);
}

// ==================== 故障代码定义 (与 STM32 一致) ====================
#define FAULT_NONE             0x00  /* 无故障 */
#define FAULT_PV_UV            0x01  /* 光伏欠压 */
#define FAULT_OC               0x02  /* 输出过流 */
#define FAULT_OT_FRONT         0x04  /* 前级过温 */
#define FAULT_OT_REAR          0x08  /* 后级过温 */
#define FAULT_OT_BOTH          0x0C  /* 前后级过温 */

// ==================== 获取故障描述 ====================
const char* getFaultString(uint8_t faultCode) {
  switch (faultCode) {
    case FAULT_NONE:       return "No Fault";
    case FAULT_PV_UV:      return "PV Undervoltage";
    case FAULT_OC:         return "Output Overcurrent";
    case FAULT_OT_FRONT:   return "Front Overtemperature";
    case FAULT_OT_REAR:    return "Rear Overtemperature";
    case FAULT_OT_BOTH:    return "Both Overtemperature";
    default:               return "Unknown Fault";
  }
}

// 文本命令缓冲区
char textBuffer[32];
uint8_t textIndex = 0;

// 事件类型定义
#define EVENT_TYPE_FAULT    0x01
#define EVENT_TYPE_WARNING  0x02
#define EVENT_TYPE_RECOVER  0x03

// ==================== 解析并发布事件帧 ====================
void parseEventFrame(const uint8_t* data, uint16_t len) {
  if (len < 6) return;
  
  uint8_t eventType = data[0];
  uint8_t faultCode = data[1];
  uint32_t faultData;
  memcpy(&faultData, &data[2], 4);
  
  // 构建事件 JSON
  StaticJsonDocument<512> doc;
  
  doc["ts"] = millis() / 1000;
  
  // 事件级别
  const char* level = "INFO";
  if (eventType == EVENT_TYPE_FAULT) level = "ERROR";
  else if (eventType == EVENT_TYPE_WARNING) level = "WARNING";
  else if (eventType == EVENT_TYPE_RECOVER) level = "RECOVER";
  doc["level"] = level;
  
  // 故障代码和描述
  doc["code"] = faultCode;
  doc["message"] = getFaultString(faultCode);
  
  // 附加数据 (如故障时的电压/电流值)
  JsonObject detail = doc.createNestedObject("detail");
  detail["rawValue"] = faultData;
  
  // 如果是电压/电流值，转换为实际值
  if (faultCode == FAULT_PV_UV) {
    detail["voltage"] = (float)faultData / 1000.0;
    detail["unit"] = "V";
  } else if (faultCode == FAULT_OC) {
    detail["current"] = (float)faultData / 1000.0;
    detail["unit"] = "A";
  } else if (faultCode == FAULT_OT_FRONT || faultCode == FAULT_OT_REAR || faultCode == FAULT_OT_BOTH) {
    detail["temperature"] = (float)faultData / 10.0;
    detail["unit"] = "°C";
  }
  
  // 发布到 event 主题
  char buffer[512];
  size_t n = serializeJson(doc, buffer);
  
  if (mqttConnected) {
    mqttClient.publish(TOPIC_EVENT, buffer);
    Serial.println("Event published to MQTT");
  }
  
  Serial.print("Event: ");
  Serial.println(buffer);
}

// ==================== 处理文本命令 ====================
void processTextCommand(const char* cmd) {
  Serial.print("Received text command: ");
  Serial.println(cmd);
  
  if (strcmp(cmd, "HB") == 0) {
    // 心跳命令，回复 ACK
    STM32_UART.println("ACK");
    Serial.println("Sent ACK to STM32");
  } else if (strcmp(cmd, "RESET") == 0) {
    // 复位命令，重新发送 READY
    delay(100);
    STM32_UART.println("READY");
    Serial.println("Sent READY to STM32");
  } else {
    Serial.println("Unknown command");
  }
}

// ==================== 处理接收到的字节 ====================
void processRxByte(uint8_t byte) {
  // 检查是否是文本命令（以可打印字符开头，不是帧头）
  if (!frameStarted && rxIndex == 0 && byte != FRAME_HEAD_1 && byte >= 0x20 && byte < 0x7F) {
    // 可能是文本命令，存入文本缓冲区
    if (byte == '\n' || byte == '\r') {
      // 命令结束
      textBuffer[textIndex] = '\0';
      if (textIndex > 0) {
        processTextCommand(textBuffer);
      }
      textIndex = 0;
    } else if (textIndex < sizeof(textBuffer) - 1) {
      textBuffer[textIndex++] = byte;
    }
    return;
  }
  
  // 二进制数据帧处理
  if (!frameStarted) {
    // 等待帧头
    if (rxIndex == 0 && byte == FRAME_HEAD_1) {
      rxBuffer[rxIndex++] = byte;
    } else if (rxIndex == 1 && byte == FRAME_HEAD_2) {
      rxBuffer[rxIndex++] = byte;
      frameStarted = true;
    } else {
      rxIndex = 0;
    }
  } else {
    // 接收数据
    if (rxIndex < FRAME_MAX_LEN) {
      rxBuffer[rxIndex++] = byte;
      
      // 检查是否接收到完整帧
      if (rxIndex >= 4) {
        uint16_t dataLen = rxBuffer[2] | (rxBuffer[3] << 8);
        uint16_t totalLen = 2 + 2 + dataLen + 1;  // head + len + data + crc
        
        if (rxIndex >= totalLen) {
          // 验证 CRC
          uint8_t receivedCrc = rxBuffer[totalLen - 1];
          uint8_t calculatedCrc = CRC8_Calculate(&rxBuffer[4], dataLen);
          
          if (receivedCrc == calculatedCrc) {
            // CRC 正确，根据数据长度判断帧类型
            if (dataLen == 72) {
              // 72字节 = 数据帧
              parseDataFrame(&rxBuffer[4], dataLen);
            } else if (dataLen == 6 && rxBuffer[4] == EVENT_TYPE_FAULT) {
              // 6字节且类型为0x01 = 事件帧
              parseEventFrame(&rxBuffer[4], dataLen);
            } else {
              Serial.println("Unknown frame type");
            }
          } else {
            Serial.println("CRC Error!");
          }
          
          // 重置接收状态
          rxIndex = 0;
          frameStarted = false;
        }
      }
    } else {
      // 缓冲区溢出，重置
      rxIndex = 0;
      frameStarted = false;
    }
  }
}

// ==================== MQTT 回调 ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  
  char message[256];
  if (length < sizeof(message)) {
    memcpy(message, payload, length);
    message[length] = '\0';
    Serial.println(message);
    
    // 转发控制命令到 STM32
    STM32_UART.println(message);
  }
}

// ==================== 初始化 ====================
void setup() {
  // 调试串口
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nESP32 MQTT Bridge Starting...");
  
  // STM32 通信串口
  STM32_UART.begin(STM32_BAUDRATE, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  
  // 连接 WiFi
  connectWiFi();
  
  // 设置 MQTT 回调
  mqttClient.setCallback(mqttCallback);
  
  // 连接 MQTT
  connectMQTT();
  
  // 发送 READY 信号给 STM32
  delay(1000);
  STM32_UART.println("READY");
  Serial.println("Sent READY to STM32");
}

// ==================== 主循环 ====================
void loop() {
  // 检查 WiFi 连接
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    mqttConnected = false;
    if (millis() - lastReconnectAttempt > 5000) {
      connectWiFi();
      lastReconnectAttempt = millis();
    }
  }
  
  // 检查 MQTT 连接
  if (wifiConnected && !mqttClient.connected()) {
    mqttConnected = false;
    if (millis() - lastReconnectAttempt > 5000) {
      connectMQTT();
      lastReconnectAttempt = millis();
    }
  }
  
  // 处理 MQTT 消息
  if (mqttConnected) {
    mqttClient.loop();
  }
  
  // 接收 STM32 数据
  while (STM32_UART.available()) {
    uint8_t byte = STM32_UART.read();
    processRxByte(byte);
  }
  
  delay(1);
}
