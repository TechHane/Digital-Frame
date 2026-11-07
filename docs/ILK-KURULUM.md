# İlk kurulum

**Klasik ESP32-WROOM / DevKit, 4 MB flash içindir.**

1. USB/güç bağlantısını çıkar. [Şemaya göre kabloları bağla](BAGLANTILAR.md).
   microSD isteğe bağlıdır; kullanacaksan FAT32 kartı güç kapalıyken tak.
2. ESP32'yi veri aktarabilen USB kablosuyla bilgisayara bağla.
3. [Python 3](https://www.python.org/downloads/) kur. ZIP'i çıkar ve paket
   klasöründe terminal aç. Şu komutu çalıştır:

   ```sh
   python -m pip install -r tools/requirements.txt
   ```

4. Aygıt Yöneticisi → Bağlantı Noktaları bölümünden **kendi ESP32 portunu** bul.
   Aşağıdaki `COM4` yalnızca örnektir; kendi portunla değiştir:

   ```sh
   python tools/flash.py --port COM4 --mode install
   ```

   **İlk kurulum ESP32'nin önceki yazılımını, ayarlarını ve dahili fotoğraflarını
   siler. SD kartı silmez.** Doğru portu seçtiğinden emin olunca `SIL` yaz.
   Bağlantı kurulamazsa “Connecting” sırasında BOOT'a basılı tut; yazma başlayınca bırak.
5. Yazma bitince gerekirse EN/RESET'e bas. Telefonu **TechHane_Cerceve** ağına
   bağla. Bu paylaşım sürümünün genel şifresi: **Techhane123**.
   “İnternet yok” uyarısında ağda kal.
6. Tarayıcıya **http://192.168.4.1** yaz. **Fotoğraf seç** ile fotoğraf ekle.
7. **Wi-Fi, şehir ve saat → Ağları tara**: kendi 2,4 GHz ağını seç, şifresini
   gir ve bağlan. Ardından il/ilçe ara, çıkan sonuca dokun.

Artık telefon bağlı kalmak zorunda değildir. Çerçeve her açılışta kayıtlı ağa
bağlanır; saati internetten alır, hava durumunu yaklaşık 15 dakikada bir yeniler.
İnternet yokken fotoğraflar çalışır; saat için **Saati telefonla eşitle** kullanılabilir.

Port görünmüyorsa kartındaki USB-seri çipinin sürücüsünü üreticisinden kur
(CP210x veya CH340 gibi). Windows'ta `python` yerine `py`, macOS/Linux'ta
`python3` gerekebilir; Linux port örneği `/dev/ttyUSB0`.

## Mevcut çerçeveyi güncellemek

Yalnızca aynı ESP32 / 4 MB / No OTA bölüm düzenindeki TechHane çerçevesi için:

```sh
python tools/flash.py --port COM4 --mode update
```

Bu komut yalnızca uygulama alanını yazar; mevcut fotoğraf ve ayarları silmez.
Farklı bir projeden geçerken **install** kullan. SD kartta eski bir TechHane
albümü varsa ilk kurulumdan sonra da görünür; boş albüm için boş kart kullan.

Kaynaktan Arduino IDE ile kurmak için [derleme kılavuzuna](DERLEME.md) bak.
Yükleme aracı [Espressif esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html) kullanır.
