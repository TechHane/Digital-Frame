# Üçüncü taraf bileşenler

Bu projenin MIT lisansı aşağıdaki bileşenlerin lisanslarının yerine geçmez.
Yazılım bu kitaplıklarla yeniden derlenebilir; tüm özgün uygulama kaynakları ve
derleme talimatları pakettedir. Kitaplıkların değiştirilmiş sürümleriyle yeniden
derleme ve bu amaçla hata ayıklama hakkı kısıtlanmaz.

| Bileşen | Sürüm / kaynak | Bildirim |
|---|---|---|
| Arduino-ESP32 | [3.3.11 kaynakları](https://github.com/espressif/arduino-esp32/tree/3.3.11) | LGPL-2.1-or-later; licenses/Arduino-ESP32-LGPL-2.1.md |
| ESP-IDF | ESP32 Arduino dağıtımının SDK'sı; [kaynak](https://github.com/espressif/esp-idf) | Apache-2.0 ve bileşen lisansları; licenses/ESP-IDF-Apache-2.0.txt |
| Adafruit GFX | [1.12.6](https://github.com/adafruit/Adafruit-GFX-Library) | BSD; licenses/Adafruit-GFX-BSD.txt |
| Adafruit ILI9341 | [1.6.3](https://github.com/adafruit/Adafruit_ILI9341) | Upstream README MIT, kaynak başlığı BSD bildirir; ikisi de licenses/Adafruit-ILI9341-notices.txt içinde korunmuştur |
| Adafruit BusIO | [1.17.4](https://github.com/adafruit/Adafruit_BusIO) | MIT; licenses/Adafruit-BusIO-MIT.txt |
| Noto Sans | [Google Fonts / Noto](https://github.com/google/fonts/tree/main/ofl/notosans) | SIL OFL 1.1; licenses/NotoSans-OFL.txt |
| esptool | [5.3.1](https://github.com/espressif/esptool) | GPL-2.0-or-later; paket içinde dağıtılmaz, pip ile kurulur |

`assets/NotoSans.ttf` değiştirilmeden dağıtılır. `smoothGlyphs` rakam/simge
rasterları bu yazı tipinden üretilmiştir ve OFL kapsamındadır. Klasik küçük
`glyphs` verisi Adafruit GFX'ten alınmıştır. Hava simgeleri projede çizilmiştir.

Saat: NTP sunucuları. Hava verisi: [Open-Meteo](https://open-meteo.com/),
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
Konum araması: [Open-Meteo Geocoding](https://open-meteo.com/en/docs/geocoding-api),
GeoNames verileri. Open-Meteo'nun ücretsiz uç noktalarının kullanım koşulları
ve sınırları geçerlidir; ticari kullanım için servisin uygun planı incelenmelidir.
