# Kaynaktan derleme

Denenen sürümler: ESP32 Arduino **3.3.11**, Adafruit GFX **1.12.6**,
Adafruit ILI9341 **1.6.3**, Adafruit BusIO **1.17.4**.

## Arduino IDE

1. [Arduino IDE](https://www.arduino.cc/en/software) kur. Tercihler → Ek Kart
   Yöneticisi URL'leri alanına şu adresi ekle:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. Kart Yöneticisi'nden **esp32 by Espressif Systems 3.3.11** kur.
   Kütüphane Yöneticisi'nden yukarıdaki üç Adafruit kütüphanesini kur.
3. `firmware/FotoCerceve/FotoCerceve.ino` dosyasını aç.
4. Kart: **ESP32 Dev Module**; Flash Size: **4 MB**; Flash Mode: **DIO**;
   Flash Frequency: **40 MHz**; Partition Scheme: **No OTA (2 MB APP / 2 MB SPIFFS)**.
5. Doğru portu seç. İlk kurulumda **Erase All Flash Before Sketch Upload: Enabled**
   (önceki verileri siler); aynı projeyi güncellerken **Disabled**. Yükle.

Hazır başlık dosyaları pakettedir; arayüzü değiştirmeden IDE ile derlemek için
Python veya yazı tipi aracı gerekmez. Menüde SPIFFS yazsa da yazılım LittleFS kullanır.

## Arduino CLI

`arduino-cli` PATH'te olmalı. Aynı çekirdek/kütüphaneler kurulu olmalıdır.
Paket kökünde:

```sh
python tools/build.py
```

CLI başka yerdeyse `--cli` ile yolunu verebilirsin. `--config`, `--libraries`
ve `--build-dir` isteğe bağlıdır. Araç `web/index.html` ve `web/bar-assets.json`
içeriğini başlıklara gömer, sonra `bin/` dosyalarını üretir.
Değişikliklerden sonra SHA256SUMS.txt yeniden oluşturulmalıdır.

Pinler ve genel kurulum ağı şifresi `Config.h` içindedir. Şifre en az 8 karakter
olmalıdır. Ev ağı bilgilerini kaynak koda yazmak yerine kurulum sayfasından gir.

Bu sürümün bilgi satırındaki rakamlar Noto Sans'tan üretilmiştir (SIL OFL 1.1).
`assets/NotoSans.ttf` ve lisansı pakettedir. Çizim verileri JSON içinde hazırdır.
Saat dilimi kodda `TRT-3` ve NTP UTC+3 olarak ayarlıdır.

Temel adresler: bootloader `0x1000`, bölüm tablosu `0x8000`, boot_app0 `0xe000`,
uygulama `0x10000`. Birleşik ilk kurulum dosyası `0x0` adresine yazılır.
