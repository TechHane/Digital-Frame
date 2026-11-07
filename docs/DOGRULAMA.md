# Paket doğrulaması — 0.3.4

- ESP32 Arduino 3.3.11 ile derlendi; uygulama 2 MB uygulama alanına sığıyor.
- Tarayıcıda boş ağ/şifre/şehir, SD/dahili hafıza metinleri, yatay/dikey yerleşim,
  bilgi satırının üç konumu, renkli rakamların aralıkları ve parlaklık kontrolü sınandı.
- Kaynaklar ve hazır yazılım dosyaları kişisel veri kalıntıları için tarandı.
- Birleşik yükleme dosyasındaki NVS ve dahili dosya sistemi alanlarının boş
  olduğu, uygulama/bootloader/bölüm tablosunun ayrı dosyalarla eşleştiği kontrol edildi.
- Şema pinleri kaynak koduyla karşılaştırıldı; PNG görseli açılarak kontrol edildi.
- Yükleme aracı install/update adresleri cihaz yazmadan (dry-run) kontrol edildi.

Temel 0.3.3 yazılımı gerçek cihazda kullanılmıştır; harici microSD + 8 GB kartın
çalıştığı kullanıcı tarafından doğrulanmıştır. **0.3.4 paylaşım derlemesi çalışan
cihaza yüklenmedi.** Boş fiziksel cihazda ilk kurulum ayrıca denenmedi.
Her ekran/modül/kablo birleşiminin parazitsiz çalışacağı garanti edilmez.

SHA256SUMS.txt, paketin dosya bütünlüğünü doğrulamak içindir; dijital imza değildir.
