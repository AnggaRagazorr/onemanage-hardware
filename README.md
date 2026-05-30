# onemanage-hardware-esp32 & LCD TFT

# ESP32 Security Patrol QR Display

## Deskripsi

Program ini digunakan untuk membuat sistem QR Code dinamis berbasis ESP32. QR Code ditampilkan pada layar TFT dan digunakan untuk kebutuhan patroli keamanan atau inspeksi area.

Saat tombol ditekan, ESP32 akan terhubung ke server melalui API untuk mengambil token. Token tersebut kemudian ditampilkan dalam bentuk QR Code. QR Code hanya aktif dalam waktu tertentu, sehingga lebih aman dibandingkan QR Code statis.

## Fungsi Utama

Program ini memiliki beberapa fungsi utama:

* Menghubungkan ESP32 ke jaringan WiFi.
* Mengambil token dari server melalui API.
* Menampilkan token dalam bentuk QR Code pada layar TFT.
* Menampilkan countdown waktu aktif QR Code.
* Menggunakan tombol untuk menampilkan QR Code.
* Menggunakan tombol tahan lama untuk restart perangkat.

## Cara Kerja

1. ESP32 dinyalakan dan mencoba terhubung ke WiFi.
2. Jika WiFi berhasil terhubung, layar menampilkan tampilan awal.
3. Saat tombol ditekan satu kali, ESP32 mengirim request ke server.
4. Server mengirim token sebagai response.
5. Token ditampilkan sebagai QR Code pada layar TFT.
6. QR Code aktif selama waktu yang ditentukan.
7. Setelah waktu habis, layar kembali ke tampilan awal.
8. Jika tombol ditahan selama 5 detik, perangkat akan restart.

## Konfigurasi

Beberapa bagian yang perlu disesuaikan sebelum digunakan:

```cpp
static const char *DEVICE_KEY = "ISI_DEVICE_KEY_DI_SINI";

const char* ssid     = "ISI_NAMA_WIFI_DI_SINI";
const char* password = "ISI_PASSWORD_WIFI_DI_SINI";
const char* apiUrl   = "https://example.com/api/token";
```

Keterangan:

* `DEVICE_KEY` digunakan sebagai identitas perangkat.
* `ssid` adalah nama WiFi.
* `password` adalah password WiFi.
* `apiUrl` adalah alamat API server untuk mengambil token.

## Komponen yang Digunakan

* ESP32
* TFT Display
* Push Button
* Koneksi WiFi
* Server/API
* Library `qrcodegen`
* Library `TFT_eSPI`
* Library `WiFi`
* Library `HTTPClient`

## Untuk menggunakan membuat qr code gunakan library qrcode.zip
