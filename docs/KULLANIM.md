# Kısa kullanım kılavuzu

**Telefondan yönet:** TechHane_Cerceve ağı → genel şifre **Techhane123** →
**http://192.168.4.1**. Ev ağına bağlandıktan sonra sayfada gösterilen yerel
IP adresini aynı ev ağı üzerinden de kullanabilirsin.

| İşlem | Ne yapmalı? |
|---|---|
| Fotoğraf ekle | **Fotoğraf seç** → telefondan bir veya birden fazla fotoğraf |
| Değiştir / sil | Albümde fotoğrafın altındaki ilgili düğme |
| Fotoğrafı değiştir | İleri/geri okları; otomatik geçiş için süre seçimi |
| Otomatik geçiş | 5 saniye–1 saat; **Duraklat / Devam et** |
| Ekranı doldur | **Alanı doldur**; kenarlar kırpılabilir |
| Tam fotoğrafı göster | **Tamamını sığdır**; boş kenarlar normaldir |
| Yönü değiştir | **Dikey / Yatay**; cihazı da aynı yöne çevir |
| Bilgi satırı | **Üstte / Altta / Gizli** |
| Yazı renkleri | Saat, tarih ve hava durumu için ayrı seçim |
| Görüntü tonları | Fotoğraf parlaklığı/kontrastı; varsayılan %100 |
| Şehir değiştir | İnternete bağlan → konum ara → sonuca dokun |

Parlaklık ayarı fotoğraf tonlarını değiştirir; fiziksel arka ışığı kısmaz.
Görünüm ve ağ ayarları yeniden açıldığında korunur.

**Depolama otomatik seçilir:** Okunabilir SD kart varsa fotoğraflar karta,
yoksa ESP32'ye kaydedilir. Sayfa hedefi ve boş alanı gösterir. Dahili sınır
**5 fotoğraf**; SD'de boş alana bağlı olarak en fazla **65.535 fotoğraf**.
Dahili ve SD albümleri ayrıdır; otomatik taşıma yapılmaz.

Kartı güç kapalıyken tak/çıkar ve yeniden başlat. FAT32 kullan; exFAT desteklenmez.
Kartın `/TechHane` klasörünü elle değiştirme. Fotoğraf yüklerken sayfayı açık tut.
JPG/PNG kullanabilirsin; HEIC desteği telefon tarayıcısına bağlıdır.

Fotoğraflar en fazla 640 piksel uzun kenar / 120.000 piksel toplam olacak şekilde
RGB565'e dönüştürülür. Telefonundaki orijinaller değişmez. Bu cihazı tek fotoğraf
yedeğin olarak kullanma.

**Sorun olursa:** Sayfa açılmıyorsa doğru ağa bağlı olduğunu ve adresin `http://`
ile başladığını kontrol et. Hava durumu için internet ve konum seçimi gerekir.
Saat elektrik kesilince kaybolursa kayıtlı Wi-Fi üzerinden yeniden eşitlenir.
SD görünmüyorsa güç kapalıyken bağlantı/FAT32 kontrolü yap. Beyaz ekran veya
parazit için kısa kablolar, ortak GND ve kararlı USB beslemesi kullan.
