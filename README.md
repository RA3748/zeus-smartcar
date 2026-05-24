# 🚗 Zeus SmartCar

Smart car berbasis **ESP8266 (NodeMCU)** yang bisa dikontrol lewat browser — support mode manual via WebSocket maupun mode otomatis line follower dengan deteksi rintangan.

---

## ✨ Fitur

- **Mode Manual** — Kontrol arah (maju, mundur, kiri, kanan, stop) lewat web interface
- **Mode Auto Line Follower** — Ikuti garis hitam di atas permukaan putih secara otomatis
- **Deteksi Rintangan** — Sensor ultrasonik berhenti otomatis jika ada halangan < 20 cm
- **Web Interface** — UI berbasis HTML/CSS/JS yang diakses langsung dari browser (tanpa app tambahan)
- **WebSocket** — Kontrol real-time dengan latensi rendah
- **Debug Serial** — Log sensor dicetak ke Serial Monitor tiap 2 detik

---

## 🛠️ Hardware yang Digunakan

| Komponen | Keterangan |
|---|---|
| **NodeMCU ESP8266** | Mikrokontroler utama (board target) |
| **Motor Driver L298N** | Driver motor DC dual channel |
| **Sensor Ultrasonik HC-SR04/05** | Deteksi jarak / rintangan |
| **IR Line Follower (x2)** | Sensor garis kiri & kanan |
| **Motor DC (x4)** | Motor penggerak roda |

---

## 📌 Pin Mapping

### Sensor Ultrasonik (HC-SR04/05)

| Pin Sensor | Pin NodeMCU | GPIO |
|---|---|---|
| ECHO | D0 | GPIO16 |
| TRIGGER | D6 | GPIO12 |

### IR Line Follower

| Sensor | Pin NodeMCU | GPIO | Logika |
|---|---|---|---|
| Sensor KIRI | D7 | GPIO13 | `0` = Putih, `1` = Hitam |
| Sensor KANAN | D5 | GPIO14 | `0` = Putih, `1` = Hitam |

### Motor Driver L298N

| Fungsi | Pin NodeMCU | GPIO |
|---|---|---|
| IN1 — Motor Kiri Forward | D1 | GPIO5 |
| IN2 — Motor Kiri Backward | D2 | GPIO4 |
| IN3 — Motor Kanan Forward | D4 | GPIO2 |
| IN4 — Motor Kanan Backward | D8 | GPIO15 |
| ENA + ENB (PWM Speed) | D3 | GPIO0 |

---

## ⚙️ Konfigurasi Speed

Nilai ini bisa di-tuning langsung di bagian atas kode:

```cpp
#define SPEED_MANUAL        200   // Kecepatan mode manual
#define SPEED_AUTO_FORWARD   90   // Kecepatan lurus di mode auto
#define SPEED_AUTO_TURN     135   // Kecepatan belok di mode auto
```

> Nilai PWM: `0` (berhenti) hingga `255` (full speed)

---

## 📶 Koneksi WiFi

ESP8266 berjalan sebagai **Access Point (AP)**. Hubungkan device kamu ke:

| Parameter | Value |
|---|---|
| SSID | `Zeus SmartCar V1` |
| Password | `zeussmartcar` |
| IP Address | `192.168.4.1` |
| Web UI | `http://192.168.4.1` |
| WebSocket | `ws://192.168.4.1:81` |

> Hanya **1 client** yang bisa terhubung dalam satu waktu.

---

## 📚 Library yang Diperlukan

Install via **Arduino Library Manager**:

| Library | Fungsi |
|---|---|
| `ESP8266WiFi` | WiFi (sudah built-in di ESP8266 core) |
| `ESP8266WebServer` | Web server HTTP |
| `WebSocketsServer` | Komunikasi WebSocket real-time |

**ESP8266 Arduino Core** juga harus sudah terinstall di Arduino IDE. Tambahkan board URL berikut di *Preferences → Additional Board Manager URLs*:
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

---

## 🚀 Cara Upload

1. Clone atau download repo ini
2. Buka `Zeus-SmartCar-V1.ino` di Arduino IDE
3. Pilih board: **NodeMCU 1.0 (ESP-12E Module)**
4. Pilih port yang sesuai
5. Upload sketch
6. Buka Serial Monitor (baud `115200`) untuk melihat log startup
7. Hubungkan HP/laptop ke WiFi `Zeus SmartCar V1`
8. Buka browser → `http://192.168.4.1`

---

## 🎮 Cara Penggunaan

### Mode Manual
Gunakan tombol arah di web interface (atau keyboard jika di desktop). Perintah dikirim via WebSocket:
- `forward` — Maju
- `backward` — Mundur
- `left` — Belok kiri
- `right` — Belok kanan
- `stop` — Berhenti

### Mode Auto Line Follower
Aktifkan dari tombol mode di web interface. Logika sensor:

| IR Kiri | IR Kanan | Aksi |
|---|---|---|
| Putih (0) | Putih (0) | Maju lurus |
| Hitam (1) | Putih (0) | Belok kiri |
| Putih (0) | Hitam (1) | Belok kanan |
| Hitam (1) | Hitam (1) | Stop (finish line / lost) |

Jika sensor ultrasonik mendeteksi rintangan < **20 cm**, mobil berhenti otomatis sampai jalur bersih kembali.

---

## 🔧 Debug

Debug aktif secara default. Lihat output di Serial Monitor (115200 baud):

```
┌─────────────────────────────────────┐
│ Mode: AUTO    | Speed: 90           │
│ IR-L: 0 (PUTIH) | IR-R: 0 (PUTIH)  │
│ Distance: 45.2 cm                   │
│ Status: ✓ On track (white)          │
└─────────────────────────────────────┘
```

Untuk menonaktifkan debug, ubah di kode:
```cpp
#define DEBUG_MODE  false
```

---

## 📁 Struktur File

```
Zeus-SmartCar-V1/
└── Zeus-SmartCar-V1.ino   # Source code utama
```

---

## 📄 Lisensi

MIT License — bebas digunakan dan dimodifikasi.
