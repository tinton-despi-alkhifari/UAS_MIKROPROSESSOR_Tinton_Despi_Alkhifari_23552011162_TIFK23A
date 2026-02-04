# ESP32 FreeRTOS MQTT Project

## Deskripsi
Proyek ini merupakan implementasi sistem embedded berbasis ESP32
yang memanfaatkan FreeRTOS untuk multitasking, komunikasi jaringan
menggunakan MQTT, serta mekanisme interupsi untuk input tombol.

## Fitur Utama
- Multitasking menggunakan FreeRTOS
- Pembacaan sensor analog (GPIO 34)
- Interupsi tombol eksternal (GPIO 4)
- Kontrol LED melalui MQTT (GPIO 2)
- Komunikasi WiFi + MQTT (MQTT Explorer)

## Arsitektur Sistem
- TaskSensor: membaca sensor setiap 5 detik
- TaskMQTT: menangani koneksi WiFi, MQTT, dan kontrol LED
- ISR tombol digunakan untuk event darurat tanpa polling

## Topik MQTT
- Publish data: `kampus/esp32/proyek_bapak/data`
- Subscribe perintah: `UAS/esp32/Mikroprosessor/perintah`

## Perintah MQTT
- `NYALA` → LED ON
- `MATI` → LED OFF

## Perangkat
- ESP32 Dev Module
- LED + resistor
- Push button
- Breadboard

## Tools
- Arduino IDE
- FreeRTOS (ESP32 built-in)
- MQTT Explorer
- HiveMQ Public Broker

## Link Video Demonstrasi
- https://youtu.be/4Sjjc3rDVvU?si=8fGi1k-1C5WV6nnz
