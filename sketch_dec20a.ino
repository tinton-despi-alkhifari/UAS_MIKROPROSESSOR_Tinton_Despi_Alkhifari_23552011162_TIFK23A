// ===========================================================================
// SKETCH FINAL: TUGAS MIKROPROSESOR ESP32 + FREERTOS
// Integrasi: Sensor Analog (D34) + Interupsi Tombol (D4) + WiFi/MQTT + Output LED (D2)
// ===========================================================================

#include <WiFi.h>
#include <PubSubClient.h>

// --- 1. KONFIGURASI PIN HARDWARE ---
#define PIN_SENSOR_ANALOG 34   // Sensor palsu (pembagi tegangan)
#define PIN_TOMBOL_EKSTERNAL 4 // Tombol di breadboard (Active Low)
#define PIN_LED_ONBOARD 2      // LED biru bawaan

// --- 2. KONFIGURASI JARINGAN (WAJIB DIGANTI!) ---
// ============================================================
const char* ssid = "Homestay2";      // <--- SSID WIFI
const char* password = "tinton123";  // <--- PASSWORD WIFI
// ============================================================

// --- 3. KONFIGURASI MQTT ---
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topik_kirim_data = "kampus/esp32/proyek_bapak/data";
const char* topik_terima_perintah = "UAS/esp32/Mikroprosessor/perintah";

// --- OBJEK GLOBAL & VARIABEL FREERTOS ---
WiFiClient espClient;
PubSubClient client(espClient);

// Handle untuk Task (Pekerja)
TaskHandle_t TaskSensorHandle = NULL;
TaskHandle_t TaskMQTTHandle = NULL;

// Queue (Antrean) untuk mengirim data sensor antar-task dengan aman
QueueHandle_t sensorQueue;
// Semaphore (Bendera) untuk sinyal interupsi tombol
SemaphoreHandle_t buttonSemaphore;

// ===========================================================================
// BAGIAN INTERUPSI & CALLBACK (Respon Cepat)
// ===========================================================================

// ISR: Fungsi kilat saat tombol ditekan
void IRAM_ATTR handleTombolISR() {
  // Beri sinyal (semaphore) ke task utama bahwa tombol ditekan.
  // "FromISR" artinya aman dipanggil dari interupsi.
  xSemaphoreGiveFromISR(buttonSemaphore, NULL);
}

// Callback MQTT: Saat ada perintah masuk dari browser
void callback(char* topic, byte* payload, unsigned int length) {
  String pesan = "";
  for (int i = 0; i < length; i++) pesan += (char)payload[i];
  Serial.printf("\n[PERINTAH MASUK] %s\n", pesan.c_str());

  // Logika Kontrol Sederhana
  if (pesan == "NYALA") {
    digitalWrite(PIN_LED_ONBOARD, HIGH);
    Serial.println("-> Aksi: LED Dinyalakan.");
  } else if (pesan == "MATI") {
    digitalWrite(PIN_LED_ONBOARD, LOW);
    Serial.println("-> Aksi: LED Dimatikan.");
  }
}

// ===========================================================================
// FUNGSI PENDUKUNG KONEKSI
// ===========================================================================
void setup_wifi() {
  delay(10);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  Serial.print("\nMenghubungkan WiFi: ");
  Serial.println(ssid);

  Serial.print("MAC ESP32: ");
  Serial.println(WiFi.macAddress());

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Konek MQTT...");
    String clientId = "ESP32-TugasAkhir-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Berhasil!");
      client.subscribe(topik_terima_perintah);
      client.publish(topik_kirim_data, "Sistem ESP32 FreeRTOS Online!");
    } else {
      Serial.print("Gagal, rc="); Serial.print(client.state()); delay(2000);
    }
  }
}

// ===========================================================================
// DEFINISI TASKS FREERTOS (PEKERJA UTAMA)
// ===========================================================================

// --- TASK 1: Pembaca Sensor (Jalan setiap 5 detik) ---
void TaskSensor(void *pvParameters) {
  int nilaiSensorRaw;
  for (;;) { // Loop tak berujung (pengganti loop() biasa)
    // 1. Baca Sensor
    nilaiSensorRaw = analogRead(PIN_SENSOR_ANALOG);
    
    // 2. Kirim nilai ke dalam Antrean (Queue) agar diambil Task MQTT
    // portMAX_DELAY artinya tunggu sampai antrean kosong jika penuh.
    xQueueSend(sensorQueue, &nilaiSensorRaw, portMAX_DELAY);
    
    // 3. Tidur selama 5 detik (vTaskDelay tidak memblokir CPU lain)
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// --- TASK 2: Manajer MQTT & Logika Utama (Jalan secepat mungkin) ---
void TaskMQTT(void *pvParameters) {
  int dataSensorDiterima;
  
  // Setup awal khusus task ini
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  for (;;) {
    // Jaga koneksi
    if (WiFi.status() != WL_CONNECTED) setup_wifi();
    if (!client.connected()) reconnect_mqtt();
    client.loop(); // Penting untuk menerima pesan MQTT

    // A. Cek apakah ada data sensor baru di Antrean?
    // (Tunggu 0ms, kalau gak ada lanjut)
    if (xQueueReceive(sensorQueue, &dataSensorDiterima, 0) == pdTRUE) {
      String pesanData = "Sensor Voltase (Raw): " + String(dataSensorDiterima);
      Serial.println("[KIRIM] " + pesanData);
      client.publish(topik_kirim_data, pesanData.c_str());
    }

    // B. Cek apakah ada sinyal Interupsi Tombol?
    // (Tunggu 0ms)
    if (xSemaphoreTake(buttonSemaphore, 0) == pdTRUE) {
       Serial.println("\n>>> ALERT: TOMBOL DARURAT DITEKAN! <<<");
       client.publish(topik_kirim_data, "ALERT: Tombol ditekan secara manual!");
       // Kedipkan LED cepat sebagai tanda
       for(int i=0;i<3;i++) { digitalWrite(PIN_LED_ONBOARD,HIGH); vTaskDelay(100/portTICK_PERIOD_MS); digitalWrite(PIN_LED_ONBOARD,LOW); vTaskDelay(100/portTICK_PERIOD_MS);}
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS); // Beri sedikit nafas untuk CPU
  }
}

// ===========================================================================
// SETUP UTAMA (Hanya jalan sekali saat boot)
// ===========================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- MEMULAI SISTEM FREERTOS ESP32 ---");

  // 1. Setup Pin
  pinMode(PIN_SENSOR_ANALOG, INPUT);
  pinMode(PIN_TOMBOL_EKSTERNAL, INPUT_PULLUP); // Penting untuk tombol active low
  pinMode(PIN_LED_ONBOARD, OUTPUT);
  digitalWrite(PIN_LED_ONBOARD, LOW);

  // 2. Setup Koneksi Awal
  setup_wifi();

  // 3. Buat Sumber Daya RTOS
  // Antrean untuk menampung maksimal 10 data integer
  sensorQueue = xQueueCreate(10, sizeof(int));
  // Semaphore biner untuk sinyal tombol
  buttonSemaphore = xSemaphoreCreateBinary();

  // 4. Pasang Interupsi
  attachInterrupt(digitalPinToInterrupt(PIN_TOMBOL_EKSTERNAL), handleTombolISR, FALLING);

  // 5. Buat dan Jalankan Task (Pekerja)
  Serial.println("Membuat Tasks...");
  // xTaskCreate(FungsiTask, "NamaBebas", UkuranStack, Parameter, Prioritas, Handle);
  xTaskCreate(TaskSensor, "SensorRead", 2048, NULL, 1, &TaskSensorHandle);
  xTaskCreate(TaskMQTT,   "MQTTManager", 4096, NULL, 2, &TaskMQTTHandle); // Prioritas lebih tinggi

  Serial.println("Sistem FreeRTOS Berjalan! Memantau...");
}

void loop() {
  // LOOP KOSONG!
  // Di FreeRTOS, loop() tidak dipakai karena tugas sudah dibagi ke TaskSensor & TaskMQTT.
}