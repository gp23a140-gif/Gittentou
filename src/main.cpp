#include <Wire.h>                   
#include <Adafruit_MPU6050.h>    
#include <Adafruit_Sensor.h>      

#include <BLEDevice.h> 
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

Adafruit_MPU6050 mpu;
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

//UUID
#define SERVICE_UUID        "01d3aaef-e02c-4998-810a-47f29479e8c0"
#define CHARACTERISTIC_UUID "71d4f0b5-bf19-4da3-bec7-9bfdc3113564"

// BLE接続
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("スマホ接続");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("スマホ切断");
  }
};

void setup() {
  Serial.begin(115200);
  delay(10000);
  Serial.println("BLE + 転倒検知スタート");

  //MPU6050 初期化
  Wire.begin(4, 5); // SDA 4番  SCL 5番
  if (!mpu.begin()) {
    Serial.println("MPU6050がない");
    while (1) delay(10);
  }
  Serial.println("MPU6050ある");

  //感度設定
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  //BLE初期化
  BLEDevice::init("ESP32C3_FALL_SENSOR");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("BLEアドバタイズ中...");
}

void loop() {
  sensors_event_t a, g, dummy;
  mpu.getEvent(&a, &g, &dummy);

  //加速度ベクトルの大きさ
  float totalAcc = sqrt(a.acceleration.x * a.acceleration.x +
                        a.acceleration.y * a.acceleration.y +
                        a.acceleration.z * a.acceleration.z);

  //ノイズ除去（移動平均）
  static float avgAcc = 9.8;
  avgAcc = 0.9 * avgAcc + 0.1 * totalAcc;

  //角度計算
  float pitch = atan2(a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;

  Serial.print("加速度: ");
  Serial.print(totalAcc, 2);
  Serial.print(" | 加速度: ");
  Serial.print(avgAcc, 2);
  Serial.print(" | 傾き前後: ");
  Serial.print(pitch, 1);
  Serial.print("° | 傾き左右: ");
  Serial.print(roll, 1);
  Serial.println("°");

  //転倒検知条件
  bool shockDetected = avgAcc > 25.0;  
  bool angleDetected = abs(pitch) > 70 || abs(roll) > 70; 

  if (shockDetected && angleDetected) {
    Serial.println("転倒検知");

    if (deviceConnected) {
      String msg = "転倒検知! Pitch=" + String(pitch, 1) + " Roll=" + String(roll, 1);
      pCharacteristic->setValue(msg.c_str());
      pCharacteristic->notify();
    }

    delay(3000);
  }

  delay(200);
}