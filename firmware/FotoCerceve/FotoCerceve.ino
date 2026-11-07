#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <SD.h>
#include <Preferences.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <sys/time.h>
#include <time.h>
#include <esp_sntp.h>
#include "Config.h"
#include "PhotoLayout.h"
#include "InfoBar.h"
#include <mbedtls/base64.h>
#include <esp_system.h>
#include "WebPage.h"

Adafruit_ILI9341 tft(&SPI, TFT_DC_PIN, TFT_CS_PIN, TFT_RST_PIN);
SPIClass sdSPI(HSPI);
GFXcanvas16 headerCanvas(320, HEADER_HEIGHT);
String lastHeaderContent;
uint32_t headerDraws = 0;
uint32_t photoDraws=0,lastPhotoDrawMs=0;
String renderedPhotoKey;
volatile bool internetTimeSynced=false;
uint32_t lastTimeAttempt=0;
WebServer server(80);
Preferences prefs;
fs::FS *photoFS=&LittleFS;
bool usingSD=false;
uint8_t clockColor=0,dateColor=0,weatherColor=0;
bool landscape = false, fillPhoto = true;
int pictureBrightness=100,pictureContrast=100;
BarPosition barPosition=BarPosition::Top;
int visibleBarHeight(){return barPosition==BarPosition::Hidden?0:HEADER_HEIGHT;}
int photoOriginY(){return barPosition==BarPosition::Top?HEADER_HEIGHT:0;}
int barOriginY(){return barPosition==BarPosition::Bottom?tft.height()-HEADER_HEIGHT:0;}
volatile int wifiReason = 0;
bool wifiScanning = false, wifiScanPending = false, wifiScanError = false;
uint32_t wifiScanAt = 0;
uint32_t connectAfter = 0;
bool geoBusy = false;
QueueHandle_t geoQueue;
String geoResult = "{\"state\":\"idle\"}";
uint32_t geoJobId = 0;
struct Weather { bool day; bool ok; float temperature; int code; uint32_t generation; } weather{};
struct WeatherJob { double lat, lon; uint32_t generation; };
QueueHandle_t weatherQueue;
bool weatherBusy = false, weatherConfigured = false;
uint32_t weatherGeneration = 0, nextWeather = 0, weatherReceived = 0;
double latitude = 0, longitude = 0;
String city, ssid, wifiPassword, apName;
bool fsReady = false, paused = false, photoDirty = true, headerDirty = true;
bool connectPending = false, connectionKnown = false;
bool webTestBusy = false;
bool httpProbeBusy = false;
int currentPhoto = -1;
uint32_t slideSeconds = 20, lastSlide = 0, lastHeader = 0, lastReconnect = 0;
File uploadFile;
bool uploading = false, uploadOK = false;
int uploadSlot = -1;
size_t uploadBytes = 0, serialTargetBytes = PHOTO_BYTES;
uint32_t uploadHash = 2166136261u;
String uploadError;
String serialLine;
bool serialReceiving = false;
uint32_t serialExpectedHash = 0, serialLastByte = 0;

String pathFor(int slot, const char *suffix = ".rgb") {
  if(usingSD){char name[40];snprintf(name,sizeof(name),"/TechHane/%02d/%03d%s",slot/1000,slot%1000,suffix);return name;}
  return "/photo" + String(slot) + suffix;
}
uint32_t hashBytes(uint32_t hash, const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; i++) hash = (hash ^ data[i]) * 16777619u;
  return hash;
}
bool readPhotoInfo(const String &path, Photo &p, bool full = true) {
  File f = photoFS->open(path, "r"); if (!f || f.size() < 8) return false;
  uint8_t header[8]; if (f.read(header,8)!=8) return false;
  p={}; p.bytes=f.size();
  if (memcmp(header,"FR02",4)==0) {
    p.width=header[4]|(header[5]<<8); p.height=header[6]|(header[7]<<8); p.offset=8;
    if (!p.width || !p.height || p.width>MAX_SOURCE_EDGE || p.height>MAX_SOURCE_EDGE || p.width*p.height>MAX_SOURCE_PIXELS || p.bytes!=8u+p.width*p.height*2u) return false;
  } else if (p.bytes==PHOTO_BYTES) { p.width=240; p.height=240; }
  else return false;
  p.cropWidth=p.width; p.cropHeight=p.height; p.present=true;
  if (!full) return true;
  f.seek(0); uint8_t block[1024]; uint32_t pos=0, hash=2166136261u;
  int minX=p.width,minY=p.height,maxX=-1,maxY=-1;
  while (pos<p.bytes) {
    size_t count=f.read(block,min(sizeof(block),static_cast<size_t>(p.bytes-pos)));
    if (!count) return false;
    hash=hashBytes(hash,block,count);
    // Old uploads contained baked-in black margins. Ignore those only for display.
    if (!p.offset) for (size_t i=0;i+1<count;i+=2) if (block[i] || block[i+1]) {
      int pixel=(pos+i)/2, x=pixel%p.width, y=pixel/p.width;
      minX=min(minX,x); minY=min(minY,y); maxX=max(maxX,x); maxY=max(maxY,y);
    }
    pos+=count; yield();
  }
  p.hash=hash;
  if (!p.offset && maxX>=minX && maxY>=minY) { p.left=minX;p.top=minY;p.cropWidth=maxX-minX+1;p.cropHeight=maxY-minY+1; }
  return true;
}
bool validFile(const String &path) { Photo p; return readPhotoInfo(path,p,false); }
#include "AlbumStorage.h"
void scanPhotos() {
  if(usingSD){fsReady=loadSDCatalog();if(!fsReady){memset(photoBits,0,sizeof(photoBits));albumCount=0;}return;}
  for(int i=0;i<INTERNAL_PHOTO_LIMIT;i++){
    internalPhotos[i]={};if(!fsReady)continue;
    String path=pathFor(i),backup=pathFor(i,".bak");
    if(photoFS->exists(backup)){
      if(!validFile(path)){photoFS->remove(path);photoFS->rename(backup,path);}
      else photoFS->remove(backup);
    }
    photoFS->remove(pathFor(i,".tmp"));
    if(!readPhotoInfo(path,internalPhotos[i]))internalPhotos[i]={};
  }
  if(fsReady)refreshStorageUsage();
}
String displayText(String text) {
  text.replace("İ","I");text.replace("ı","i");text.replace("ş","s");text.replace("Ş","S");
  text.replace("ğ","g");text.replace("Ğ","G");text.replace("ü","u");text.replace("Ü","U");
  text.replace("ö","o");text.replace("Ö","O");text.replace("ç","c");text.replace("Ç","C");
  return text;
}
String wifiState() {
  if (WiFi.status()==WL_CONNECTED) return "connected";
  if (!ssid.length()) return "not_configured";
  if (wifiReason==201) return "not_found";
  if (wifiReason==202 || wifiReason==204 || wifiReason==15) return "password_error";
  return "connecting";
}
void advancePhoto(int direction) {
  if (!photoCount()) { currentPhoto = -1; photoDirty = true; return; }
  int start = currentPhoto;
  if (start < 0) start = direction > 0 ? photoLimit() - 1 : 0;
  for (int n = 1; n <= photoLimit(); n++) {
    int slot = (start + direction * n + photoLimit() * 2) % photoLimit();
    if (photoPresent(slot)) { currentPhoto = slot; break; }
  }
  photoDirty = true; lastSlide = millis();
}
void drawText(const String &text, int x, int y, uint16_t color, uint8_t size = 1) {
  tft.setFont(nullptr); tft.setTextSize(size); tft.setTextColor(color); tft.setCursor(x, y); tft.print(text);
}
void renderHeader() {
  if(barPosition==BarPosition::Hidden)return;
  time_t now=time(nullptr);tm local{};localtime_r(&now,&local);
  char clockText[8]="--:--",dateText[16]="--.--.--";
  if(now>1700000000) { strftime(clockText,sizeof(clockText),"%H:%M",&local);strftime(dateText,sizeof(dateText),"%d.%m.%y",&local); }
  String temp=weather.ok?String(static_cast<int>(roundf(weather.temperature))):"--";
  int icon=weatherIcon(weather.code,weather.day,weather.ok);
  String content=String(clockText)+dateText+temp+String(icon)+String(landscape)+barName(barPosition)+String(clockColor)+String(dateColor)+String(weatherColor);
  if(content==lastHeaderContent || !headerCanvas.getBuffer())return;
  lastHeaderContent=content;
  drawInfoBar(headerCanvas,tft.width(),clockText,dateText,temp,icon,color565(clockColor),color565(dateColor),color565(weatherColor));
  // Transfer only the visible strip; canvas rows retain their 320-pixel stride.
  tft.startWrite();tft.setAddrWindow(0,barOriginY(),tft.width(),HEADER_HEIGHT);
  for(int y=0;y<HEADER_HEIGHT;y++)tft.writePixels(headerCanvas.getBuffer()+320*y,tft.width(),true,false);
  tft.endWrite();headerDraws++;
}
void renderPhoto() {
  if (uploading) return;
  photoDirty=false;
  int width=tft.width(),height=tft.height()-visibleBarHeight(),originY=photoOriginY();
  if (currentPhoto<0 || !photoPresent(currentPhoto)) {
    renderedPhotoKey="";
    tft.fillRect(0,originY,width,height,0x0842);
    drawText("FOTO CERCEVE",16,70,ILI9341_WHITE,2);
    drawText(apName,16,113,0x7E5B);
    drawText("Telefondan fotograf ekle.",16,145,0xA534);
    return;
  }
  Photo p=photoInfo(currentPhoto);
  String drawKey=String(currentPhoto)+":"+String(p.hash)+":"+String(width)+":"+String(height)+":"+String(originY)+":"+String(fillPhoto)+":"+String(pictureBrightness)+":"+String(pictureContrast);
  if(p.present&&drawKey==renderedPhotoKey)return;
  uint32_t drawStarted=millis();
  File f=photoFS->open(pathFor(currentPhoto),"r");
  if (!f || !p.present) { storageError="Fotograf okunamadi. SD karti ve baglantilari kontrol et.";return; }
  PhotoView v=photoView(p,width,height,fillPhoto);
  static uint16_t sourceA[MAX_SOURCE_EDGE],sourceB[MAX_SOURCE_EDGE],output[320*4];
  uint16_t mapR[32],mapG[64],mapB[32];
  for(int channel=0;channel<3;channel++){
    int maximum=channel==1?63:31,shift=channel==0?11:channel==1?5:0;
    for(int value=0;value<=maximum;value++){
      int scaled=value;
      if(pictureBrightness!=100||pictureContrast!=100){
        int v=(value*255+maximum/2)/maximum;
        v=((v-128)*pictureContrast/100+128)*pictureBrightness/100;
        scaled=(constrain(v,0,255)*maximum+127)/255;
      }
      if(channel==0)mapR[value]=scaled<<shift;else if(channel==1)mapG[value]=scaled<<shift;else mapB[value]=scaled;
    }
  }
  int cachedA=-1,cachedB=-1;
  renderedPhotoKey=""; // A partial/failed draw must never count as a complete frame.
  // Bilinear interpolation avoids the blocky nearest-neighbour enlargement.
  for(int y=0;y<height;y+=4) {
    int rows=min(4,height-y);
    for(int row=0;row<rows;row++) {
      int dy=y+row;bool inY=dy>=v.dy && dy<v.dy+v.dh;
      int sy=0,sy1=0,fy=0;
      if(inY) {
        int fixedY=v.dh>1?(dy-v.dy)*(v.sh-1)*256/(v.dh-1):0;
        sy=v.sy+(fixedY>>8);sy1=min(v.sy+v.sh-1,sy+1);fy=fixedY&255;
        if(sy!=cachedA) { f.seek(p.offset+sy*p.width*2);if(f.read(reinterpret_cast<uint8_t*>(sourceA),p.width*2)!=p.width*2){storageError="Fotograf okunamadi; karti ve baglantilari kontrol et.";return;}cachedA=sy; }
        if(sy1!=cachedB) { f.seek(p.offset+sy1*p.width*2);if(f.read(reinterpret_cast<uint8_t*>(sourceB),p.width*2)!=p.width*2){storageError="Fotograf okunamadi; karti ve baglantilari kontrol et.";return;}cachedB=sy1; }
      }
      for(int x=0;x<width;x++) {
        uint16_t color=0;
        if(inY && x>=v.dx && x<v.dx+v.dw) {
          int fixedX=v.dw>1?(x-v.dx)*(v.sw-1)*256/(v.dw-1):0;
          int sx=v.sx+(fixedX>>8),sx1=min(v.sx+v.sw-1,sx+1),fx=fixedX&255;
          uint16_t a=sourceA[sx],b=sourceA[sx1],c=sourceB[sx],d=sourceB[sx1];
          for(int channel=0;channel<3;channel++) {
            int shift=channel==0?11:channel==1?5:0,mask=channel==1?63:31;
            int top=((a>>shift)&mask)*(256-fx)+((b>>shift)&mask)*fx;
            int bottom=((c>>shift)&mask)*(256-fx)+((d>>shift)&mask)*fx;
            int value=(top*(256-fy)+bottom*fy+32768)>>16;color|=value<<shift;
          }
        }
        if(inY&&x>=v.dx&&x<v.dx+v.dw)color=mapR[color>>11]|mapG[(color>>5)&63]|mapB[color&31];
        output[row*width+x]=color;
      }
    }
    tft.startWrite();tft.setAddrWindow(0,originY+y,width,rows);tft.writePixels(output,width*rows,true,false);tft.endWrite();yield();
  }
  renderedPhotoKey=drawKey;photoDraws++;lastPhotoDrawMs=millis()-drawStarted;
}

bool beginUpload(int slot) {
  uploadOK=false;uploadError="";
  if(uploading){uploadError="Baska bir yukleme devam ediyor.";return false;}
  if(usingSD&&slot==-1)slot=nextFreeSlot();
  if(!fsReady||slot<0||slot>=photoLimit()){uploadError="Album dolu veya hafiza hazir degil.";return false;}
  if(usingSD&&!photoPresent(slot)&&slot!=nextFreeSlot()){uploadError="Yeni fotograf icin siradaki bos alani kullan.";return false;}
  if(usingSD&&photoFS->exists(JOURNAL_PATH)){uploadError="SD islemi yarim kaldi. Cerceveyi yeniden baslat.";return false;}
  refreshStorageUsage();
  if(storageTotal<storageUsed+MAX_PHOTO_BYTES+65536){uploadError="Yeterli bos alan yok; kayitli fotograflar korundu.";return false;}
  if(!preparePhotoFolder(slot)||!startStorageJournal(slot)){uploadError="SD kart yazmaya hazir degil.";return false;}
  uploadSlot=slot;uploadBytes=0;uploadHash=2166136261u;
  uploadFile=photoFS->open(pathFor(slot,".tmp"),"w");
  if(!uploadFile){uploadError="Dosya olusturulamadi.";finishStorageJournal();return false;}
  uploading=true;return true;
}
void appendUpload(const uint8_t *data, size_t size) {
  if (!uploading || !uploadError.isEmpty()) return;
  if (uploadBytes + size > MAX_PHOTO_BYTES) { uploadError = "Fotograf boyutu gecersiz."; return; }
  if (uploadFile.write(data, size) != size) { uploadError = "Hafizaya yazilamadi."; return; }
  uploadBytes += size; uploadHash = hashBytes(uploadHash, data, size);
}
bool finishUpload(bool cancelled = false) {
  if(!uploading)return false;
  uploadFile.flush();uploadFile.close();
  String path=pathFor(uploadSlot),temp=pathFor(uploadSlot,".tmp"),backup=pathFor(uploadSlot,".bak");
  Photo incoming{};
  if(cancelled||!readPhotoInfo(temp,incoming)||incoming.hash!=uploadHash)uploadError="Yukleme tamamlanamadi; onceki fotograf korundu.";
  bool renamed=false;
  if(uploadError.isEmpty()){
    bool hadOld=photoFS->exists(path);
    if(photoFS->exists(backup))uploadError="Onceki islem tamamlanmamis. Cerceveyi yeniden baslat.";
    if(uploadError.isEmpty()&&hadOld&&!photoFS->rename(path,backup))uploadError="Eski dosya korunamadi.";
    if(uploadError.isEmpty()){
      if(!photoFS->rename(temp,path)){
        uploadError="Fotograf kaydedilemedi.";
        if(hadOld)photoFS->rename(backup,path);
      }else renamed=true;
    }
    if(renamed&&setPhotoInfo(uploadSlot,incoming)){
      photoFS->remove(backup);finishStorageJournal();
      currentPhoto=uploadSlot;photoDirty=true;lastSlide=millis();uploadOK=true;
    }else if(renamed)uploadError="SD album kaydi tamamlanamadi. Cerceveyi yeniden baslat.";
  }
  // Keep the journal and backup when a commit failed; boot recovery resolves it.
  photoFS->remove(temp);
  if(!renamed&&!photoFS->exists(backup))finishStorageJournal();
  uploading=false;refreshStorageUsage();return uploadOK;
}
String jsonString(cJSON *root) {
  char *text = cJSON_PrintUnformatted(root); String result = text ? text : "{}"; cJSON_free(text); cJSON_Delete(root); return result;
}
cJSON *photoJSON(int slot){
  Photo p=photoInfo(slot);cJSON *item=cJSON_CreateObject();
  cJSON_AddNumberToObject(item,"slot",slot);cJSON_AddBoolToObject(item,"present",p.present);
  char hash[12];snprintf(hash,sizeof(hash),"%08lx",static_cast<unsigned long>(p.hash));cJSON_AddStringToObject(item,"hash",hash);
  cJSON_AddNumberToObject(item,"bytes",p.bytes);cJSON_AddNumberToObject(item,"width",p.width);cJSON_AddNumberToObject(item,"height",p.height);
  cJSON_AddNumberToObject(item,"cropWidth",p.cropWidth);cJSON_AddNumberToObject(item,"cropHeight",p.cropHeight);return item;
}
int requestedPage(){return max(0,static_cast<int>(server.arg("page").toInt()));}
String statusJSON(int page = 0) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "version", VERSION);
  cJSON_AddBoolToObject(root, "fsReady", fsReady);
  cJSON_AddNumberToObject(root, "flashBytes", ESP.getFlashChipSize());
  cJSON_AddNumberToObject(root, "freeHeap", ESP.getFreeHeap());
  cJSON_AddNumberToObject(root, "totalBytes", fsReady ? storageTotal : 0);
  cJSON_AddNumberToObject(root, "usedBytes", fsReady ? storageUsed : 0);
  cJSON_AddNumberToObject(root, "current", currentPhoto);
  cJSON_AddNumberToObject(root, "interval", slideSeconds);
  cJSON_AddNumberToObject(root,"brightness",pictureBrightness);cJSON_AddNumberToObject(root,"contrast",pictureContrast);
  cJSON_AddBoolToObject(root, "paused", paused);
  cJSON_AddBoolToObject(root, "connected", WiFi.status() == WL_CONNECTED);
  cJSON_AddStringToObject(root, "ip", WiFi.localIP().toString().c_str());
  cJSON_AddStringToObject(root, "ap", apName.c_str());
  cJSON_AddStringToObject(root, "apIP", WiFi.softAPIP().toString().c_str());
  cJSON_AddNumberToObject(root, "apClients", WiFi.softAPgetStationNum());
  cJSON_AddNumberToObject(root, "uptimeSeconds", millis() / 1000);
  cJSON_AddStringToObject(root, "ssid", ssid.c_str());
  cJSON_AddStringToObject(root, "city", city.c_str());
  cJSON_AddBoolToObject(root, "weatherConfigured", weatherConfigured);
  cJSON_AddBoolToObject(root, "weatherOK", weather.ok);
  if (weather.ok) cJSON_AddNumberToObject(root, "temperature", weather.temperature);
  cJSON_AddBoolToObject(root,"internetTimeSynced",internetTimeSynced);
  cJSON_AddNumberToObject(root,"weatherAgeSeconds",weather.ok?(millis()-weatherReceived)/1000:-1);
  cJSON_AddNumberToObject(root, "epoch", static_cast<double>(time(nullptr)));
  cJSON_AddNumberToObject(root, "headerHeight", visibleBarHeight());
  cJSON_AddStringToObject(root, "barPosition", barName(barPosition));
  cJSON_AddNumberToObject(root,"headerY",barOriginY());
  cJSON_AddNumberToObject(root,"photoY",photoOriginY());
  cJSON_AddNumberToObject(root,"photoHeight",tft.height()-visibleBarHeight());
  cJSON_AddNumberToObject(root,"weatherCode",weather.code);
  int icon=weatherIcon(weather.code,weather.day,weather.ok);
  cJSON_AddNumberToObject(root,"weatherIcon",icon);
  cJSON_AddStringToObject(root,"weatherLabel",conditionName(icon));
  cJSON_AddNumberToObject(root, "headerDraws", headerDraws);
  cJSON_AddNumberToObject(root,"photoDraws",photoDraws);
  cJSON_AddNumberToObject(root,"photoDrawMs",lastPhotoDrawMs);
  cJSON_AddNumberToObject(root,"displaySPIHz",TFT_SPEED);
  cJSON_AddNumberToObject(root,"sdIndexReads",sdIndexReads);
  cJSON_AddNumberToObject(root, "resetReason", esp_reset_reason());
  cJSON_AddStringToObject(root, "orientation", landscape ? "landscape" : "portrait");
  cJSON_AddStringToObject(root, "fit", fillPhoto ? "cover" : "contain");
  cJSON_AddNumberToObject(root, "screenWidth", tft.width());
  cJSON_AddNumberToObject(root, "screenHeight", tft.height());
  cJSON_AddStringToObject(root, "wifiState", wifiState().c_str());
  cJSON_AddNumberToObject(root, "wifiReason", wifiReason);
  cJSON_AddBoolToObject(root, "hasPassword", wifiPassword.length()>0);
  cJSON_AddNumberToObject(root, "lat", latitude);cJSON_AddNumberToObject(root, "lon", longitude);
  cJSON_AddStringToObject(root,"storage",usingSD?"sd":"internal");
  cJSON_AddStringToObject(root,"storageError",storageError.c_str());
  cJSON_AddNumberToObject(root,"photoCount",photoCount());cJSON_AddNumberToObject(root,"photoLimit",photoLimit());
  cJSON_AddNumberToObject(root,"nextSlot",nextFreeSlot());
  cJSON_AddStringToObject(root,"clockColor",COLOR_NAMES[clockColor]);cJSON_AddStringToObject(root,"dateColor",COLOR_NAMES[dateColor]);cJSON_AddStringToObject(root,"weatherColor",COLOR_NAMES[weatherColor]);
  int pages=usingSD?max(1,(photoCount()+ALBUM_PAGE_SIZE-1)/ALBUM_PAGE_SIZE):1;page=constrain(page,0,pages-1);
  cJSON_AddNumberToObject(root,"page",page);cJSON_AddNumberToObject(root,"pageCount",pages);
  cJSON_AddItemToObject(root,"activePhoto",photoJSON(currentPhoto));
  cJSON *items=cJSON_AddArrayToObject(root,"photos");int seen=0,added=0;
  for(int i=0;i<photoLimit();i++){
    if(usingSD){if(!photoPresent(i))continue;if(seen++<page*ALBUM_PAGE_SIZE)continue;if(added++>=ALBUM_PAGE_SIZE)break;}
    cJSON_AddItemToArray(items,photoJSON(i));
  }
  return jsonString(root);
}
void sendError(int code, const String &message) {
  cJSON *root = cJSON_CreateObject(); cJSON_AddStringToObject(root, "error", message.c_str());
  server.send(code, "application/json; charset=utf-8", jsonString(root));
}
bool localMutation() {
  // A foreign page cannot add this header without a CORS preflight; we grant no CORS.
  if (server.header("X-Frame-Request") != "1") { sendError(403, "Yonetim sayfasindan tekrar deneyin."); return false; }
  return true;
}
int requestedSlot() {
  String value=server.arg("slot");if(!value.length()||value.length()>5)return -2;
  for(size_t i=0;i<value.length();i++)if(value[i]<'0'||value[i]>'9')return -2;
  int slot=value.toInt();return slot<photoLimit()?slot:-2;
}
void handleControl() {
  if (!localMutation()) return;
  String action = server.arg("action");
  if (action == "next") advancePhoto(1);
  else if (action == "previous") advancePhoto(-1);
  else if (action == "pause") { paused = !paused; lastSlide = millis(); }
  else if (action == "show") {
    int slot = requestedSlot(); if (slot < 0 || !photoPresent(slot)) { sendError(400, "Fotograf bulunamadi."); return; }
    currentPhoto = slot; photoDirty = true; lastSlide = millis();
  } else { sendError(400, "Gecersiz islem."); return; }
  server.send(200, "application/json", statusJSON(requestedPage()));
}
void handleSettings() {
  if (!localMutation()) return;
  if (uploading) { sendError(409,"Fotograf yuklemesi bitsin.");return; }
  String inputSsid=server.hasArg("ssid")?server.arg("ssid"):ssid;
  String password=server.arg("password");
  bool networkRequested=server.hasArg("ssid");
  bool keepPassword=server.arg("keepPassword")=="1" || (inputSsid==ssid && !password.length() && server.arg("open")!="1");
  if (keepPassword) password=wifiPassword;
  if (inputSsid.length()>32 || password.length()>63 || (password.length() && password.length()<8)) { sendError(400,"Wi-Fi sifresi en az 8 karakter olmali.");return; }
  int duration=server.hasArg("interval")?server.arg("interval").toInt():slideSeconds;
  if (duration<5 || duration>3600) { sendError(400,"Sure 5-3600 saniye olmali.");return; }
  String fit=server.hasArg("fit")?server.arg("fit"):(fillPhoto?"cover":"contain");
  String orientation=server.hasArg("orientation")?server.arg("orientation"):(landscape?"landscape":"portrait");
  if ((fit!="cover" && fit!="contain") || (orientation!="portrait" && orientation!="landscape")) { sendError(400,"Gorunum ayari gecersiz.");return; }
  String position=server.hasArg("barPosition")?server.arg("barPosition"):barName(barPosition);
  if(position!="top" && position!="bottom" && position!="hidden") { sendError(400,"Bilgi satiri konumu gecersiz.");return; }
  int nextClock=server.hasArg("clockColor")?colorIndex(server.arg("clockColor")):clockColor;
  int nextDate=server.hasArg("dateColor")?colorIndex(server.arg("dateColor")):dateColor;
  int nextWeatherColor=server.hasArg("weatherColor")?colorIndex(server.arg("weatherColor")):weatherColor;
  if(nextClock<0||nextDate<0||nextWeatherColor<0){sendError(400,"Renk secimi gecersiz.");return;}
  int nextBrightness=pictureBrightness,nextContrast=pictureContrast;
  for(int i=0;i<2;i++){
    const char *key=i?"contrast":"brightness";
    if(server.hasArg(key)){
      String value=server.arg(key);char *end;long parsed=strtol(value.c_str(),&end,10);
      if(!value.length()||*end||parsed<(i?50:20)||parsed>150){sendError(400,"Parlaklik veya kontrast degeri gecersiz.");return;}
      if(i)nextContrast=parsed;else nextBrightness=parsed;
    }
  }
  time_t epoch=0;
  if (server.hasArg("epoch")) {
    char *end;String value=server.arg("epoch");epoch=strtoll(value.c_str(),&end,10);
    if (*end || epoch<=1700000000 || epoch>=4102444800LL) { sendError(400,"Telefonun tarihini kontrol et.");return; }
  }
  double lat=latitude,lon=longitude;String nextCity=city;
  if (server.hasArg("city")) {
    nextCity=server.arg("city");nextCity.trim();
    if (!nextCity.length() || nextCity.length()>80 || !server.hasArg("lat") || !server.hasArg("lon")) { sendError(400,"Arama sonuclarindan konum sec.");return; }
    char *a,*b;String la=server.arg("lat"),lo=server.arg("lon");lat=strtod(la.c_str(),&a);lon=strtod(lo.c_str(),&b);
    if (!la.length() || !lo.length() || *a || *b || !isfinite(lat) || !isfinite(lon) || abs(lat)>90 || abs(lon)>180) { sendError(400,"Konum gecersiz.");return; }
  }
  if (!server.args()) { sendError(400,"Ayar alinamadi.");return; }
  if (networkRequested) {
    if (ssid!=inputSsid || password!=wifiPassword) { ssid=inputSsid;wifiPassword=password;prefs.putString("ssid",ssid);prefs.putString("pass",wifiPassword); }
    wifiReason=0;connectPending=true;connectAfter=millis()+500;
  }
  if (slideSeconds!=static_cast<uint32_t>(duration)) { slideSeconds=duration;prefs.putUInt("interval",slideSeconds); }
  if (server.hasArg("city")) {
    latitude=lat;longitude=lon;city=nextCity;weatherConfigured=true;
    prefs.putDouble("lat",lat);prefs.putDouble("lon",lon);prefs.putString("city",city);prefs.putBool("weather",true);
    weatherGeneration++;weather.ok=false;nextWeather=0;
  }
  bool nextLandscape=orientation=="landscape",nextFill=fit=="cover";
  if (nextLandscape!=landscape) { landscape=nextLandscape;prefs.putBool("landscape",landscape);tft.setRotation(landscape?1:0);lastHeaderContent="";photoDirty=true; }
  if (nextFill!=fillPhoto) { fillPhoto=nextFill;prefs.putBool("fill",fillPhoto);photoDirty=true; }
  BarPosition nextPosition=parseBarPosition(position);
  if(nextPosition!=barPosition) { barPosition=nextPosition;prefs.putString("bar",position);lastHeaderContent="";photoDirty=true; }
  if(pictureBrightness!=nextBrightness){pictureBrightness=nextBrightness;prefs.putUInt("brightness",pictureBrightness);photoDirty=true;}
  if(pictureContrast!=nextContrast){pictureContrast=nextContrast;prefs.putUInt("contrast",pictureContrast);photoDirty=true;}
  if(clockColor!=nextClock){clockColor=nextClock;prefs.putUChar("clockCol",clockColor);}
  if(dateColor!=nextDate){dateColor=nextDate;prefs.putUChar("dateCol",dateColor);}
  if(weatherColor!=nextWeatherColor){weatherColor=nextWeatherColor;prefs.putUChar("weatherCol",weatherColor);}
  if (epoch) { timeval value{epoch,0};settimeofday(&value,nullptr); }
  headerDirty=true;lastSlide=millis();server.send(200,"application/json; charset=utf-8",statusJSON(requestedPage()));
}
esp_err_t weatherEvent(esp_http_client_event_t *event) {
  if (event->event_id == HTTP_EVENT_ON_DATA) {
    String *body = static_cast<String *>(event->user_data);
    if (body->length() + event->data_len > 32768) return ESP_FAIL;
    body->concat(static_cast<const char *>(event->data), event->data_len);
  }
  return ESP_OK;
}
void weatherWork(void *data) {
  WeatherJob job = *static_cast<WeatherJob *>(data); delete static_cast<WeatherJob *>(data);
  Weather result{}; result.generation = job.generation;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(job.lat, 4) + "&longitude=" + String(job.lon, 4) + "&current=temperature_2m,weather_code,is_day&timezone=Europe%2FIstanbul";
  String body; body.reserve(2048);
  esp_http_client_config_t config{}; config.url = url.c_str(); config.crt_bundle_attach = esp_crt_bundle_attach;
  config.timeout_ms = 10000; config.event_handler = weatherEvent; config.user_data = &body; config.buffer_size = 1024;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client) {
    if (esp_http_client_perform(client) == ESP_OK && esp_http_client_get_status_code(client) == 200) {
      cJSON *root = cJSON_Parse(body.c_str());
      if (root) {
        cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
        cJSON *temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
        cJSON *code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
        if (cJSON_IsNumber(temperature) && cJSON_IsNumber(code)) { result.ok = true; result.temperature = temperature->valuedouble; result.code = code->valueint; cJSON *day=cJSON_GetObjectItemCaseSensitive(current,"is_day");result.day=!cJSON_IsNumber(day)||day->valueint==1; }
        cJSON_Delete(root);
      }
    }
    esp_http_client_cleanup(client);
  }
  xQueueOverwrite(weatherQueue, &result);
}
// Return from the worker so C++ buffers are freed before deleting the RTOS task.
void weatherTask(void *data) { weatherWork(data); vTaskDelete(nullptr); }
void serviceWeather() {
  Weather result;
  if (xQueueReceive(weatherQueue, &result, 0) == pdTRUE) {
    weatherBusy = false;
    if (result.generation == weatherGeneration) {
      if (result.ok) { weather = result; weatherReceived = millis(); }
      nextWeather = millis() + (result.ok ? 15UL * 60 * 1000 : 60UL * 1000);
    } else nextWeather = 0;
    headerDirty = true;
  }
  if (!weatherBusy && !geoBusy && !uploading && weatherConfigured && WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000 && (!nextWeather || static_cast<int32_t>(millis() - nextWeather) >= 0)) {
    WeatherJob *job = new WeatherJob{latitude, longitude, weatherGeneration};
    weatherBusy = true;
    if (xTaskCreate(weatherTask, "weather", 8192, job, 1, nullptr) != pdPASS) { delete job; weatherBusy = false; nextWeather = millis() + 60000; }
  }
}
#include "Services.h"
void setupRoutes() {
  const char *headers[] = {"X-Frame-Request"}; server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, [] { server.sendHeader("Cache-Control", "no-store"); server.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  server.on("/api/status", HTTP_GET, [] { server.sendHeader("Cache-Control", "no-store"); server.send(200, "application/json; charset=utf-8", statusJSON(requestedPage())); });
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/photo", HTTP_GET, [] {
    int slot = requestedSlot();
    if (!fsReady || slot < 0 || !photoPresent(slot)) { sendError(404, "Fotograf bulunamadi."); return; }
    sendPhotoChunk(slot, false);
  });
  server.on("/api/thumbnail", HTTP_GET, [] {
    int slot=requestedSlot();
    if (slot<0 || !photoPresent(slot)) { sendError(404,"Fotograf bulunamadi.");return; }
    sendPhotoChunk(slot,true);
  });
  server.on("/api/wifi/scan",HTTP_POST,startWifiScan);
  server.on("/api/wifi/scan",HTTP_GET,sendWifiScan);
  server.on("/api/cities",HTTP_POST,startGeoSearch);
  server.on("/api/cities",HTTP_GET,[] { server.send(200,"application/json; charset=utf-8",geoResult); });
  server.on("/api/delete", HTTP_POST, [] {
    if (!localMutation()) return;
    int slot = requestedSlot();
    if (slot < 0 || !fsReady || uploading) { sendError(400, "Islem yapilamadi."); return; }
    if (!deleteStoredPhoto(slot)) { sendError(500, "Fotograf silinemedi."); return; }
    if (slot == currentPhoto) advancePhoto(1);
    server.send(200, "application/json", statusJSON(requestedPage()));
  });
  server.on("/api/upload", HTTP_POST, [] {
    if (!localMutation()) return;
    if (uploadOK) {
      cJSON *root=cJSON_Parse(statusJSON(requestedPage()).c_str());
      cJSON_AddNumberToObject(root,"uploadedSlot",uploadSlot);cJSON_AddItemToObject(root,"uploadedPhoto",photoJSON(uploadSlot));
      server.send(200,"application/json",jsonString(root));
    }
    else sendError(400, uploadError.length() ? uploadError : "Fotograf alinamadi.");
    uploadOK = false;
  }, [] {
    HTTPUpload &u = server.upload();
    if (u.status == UPLOAD_FILE_START) {
      uploadOK = false;
      if (server.header("X-Frame-Request") != "1" || serialReceiving) { uploadError = "Yukleme reddedildi."; return; }
      beginUpload(usingSD&&!server.hasArg("slot")?-1:requestedSlot());
    } else if (u.status == UPLOAD_FILE_WRITE) appendUpload(u.buf, u.currentSize);
    else if (u.status == UPLOAD_FILE_END) finishUpload();
    else if (u.status == UPLOAD_FILE_ABORTED) finishUpload(true);
  });
  server.onNotFound([] { server.send(404, "text/plain; charset=utf-8", "Sayfa bulunamadi."); });
  server.begin();
}
// Probe the actual web server without changing Wi-Fi, photographs or settings.
// Runs separately so the main loop can accept and serve this HTTP connection.
void webTestWork() {
  String url = "http://127.0.0.1/";
  esp_http_client_config_t config{};
  config.url = url.c_str(); config.timeout_ms = 5000;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t error = client ? esp_http_client_perform(client) : ESP_ERR_NO_MEM;
  int code = client ? esp_http_client_get_status_code(client) : 0;
  int length = client ? esp_http_client_get_content_length(client) : 0;
  if (client) esp_http_client_cleanup(client);
  Serial.printf("WEBTEST error=%s status=%d bytes=%d\n", esp_err_to_name(error), code, length);
  webTestBusy = false;
}
void webTestTask(void *) { webTestWork(); vTaskDelete(nullptr); }
// USB-only diagnostic: bounded local HTTP requests, including photo regression tests.
void httpProbeWork(void *data) {
  String request = *static_cast<String *>(data); delete static_cast<String *>(data);
  cJSON *root = cJSON_Parse(request.c_str());
  cJSON *path = cJSON_GetObjectItemCaseSensitive(root, "path");
  cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "body");
  cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
  cJSON *auth = cJSON_GetObjectItemCaseSensitive(root, "auth");
  cJSON *binary = cJSON_GetObjectItemCaseSensitive(root, "bodyBase64");
  String resultBody, url;
  String route=cJSON_IsString(path)?String(path->valuestring):"";
  int queryIndex=route.indexOf('?');if(queryIndex>=0)route=route.substring(0,queryIndex);
  bool allowed = route=="/api/status" || route=="/api/control" || route=="/api/settings" || route=="/api/photo" || route=="/api/thumbnail" || route=="/api/delete" || route=="/api/upload" || route=="/api/wifi/scan" || route=="/api/cities";
  std::unique_ptr<unsigned char[]> decoded;size_t decodedLength=0;
  if(cJSON_IsString(binary)) {
    size_t capacity=strlen(binary->valuestring)*3/4+4;
    if(capacity>8192)allowed=false;
    else { decoded.reset(new unsigned char[capacity]);if(mbedtls_base64_decode(decoded.get(),capacity,&decodedLength,reinterpret_cast<unsigned char*>(binary->valuestring),strlen(binary->valuestring))!=0)allowed=false; }
  }
  esp_err_t error = ESP_ERR_INVALID_ARG; int code = 0;
  if (allowed) {
    url = "http://127.0.0.1" + String(path->valuestring);
    esp_http_client_config_t config{}; config.url = url.c_str(); config.timeout_ms = 5000;
    config.event_handler = weatherEvent; config.user_data = &resultBody;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client) {
      if (cJSON_IsString(body) || decoded) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", cJSON_IsString(type) ? type->valuestring : "application/x-www-form-urlencoded");
        if (!cJSON_IsFalse(auth)) esp_http_client_set_header(client, "X-Frame-Request", "1");
        esp_http_client_set_post_field(client, decoded?reinterpret_cast<char*>(decoded.get()):body->valuestring, decoded?decodedLength:strlen(body->valuestring));
      }
      error = esp_http_client_perform(client); code = esp_http_client_get_status_code(client);
      esp_http_client_cleanup(client);
    }
  }
  cJSON_Delete(root);
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "error", esp_err_to_name(error));
  cJSON_AddNumberToObject(result, "status", code); cJSON_AddStringToObject(result, "body", resultBody.c_str());
  Serial.println("HTTPRESULT " + jsonString(result));
  httpProbeBusy = false;
}
void httpProbeTask(void *data) { httpProbeWork(data); vTaskDelete(nullptr); }
void processSerialLine(const String &line) {
  if (line == "STATUS") Serial.println(statusJSON());
  else if (line.startsWith("HTTP ") && !httpProbeBusy) {
    String *request = new String(line.substring(5)); httpProbeBusy = true;
    if (xTaskCreate(httpProbeTask, "httpprobe", 8192, request, 1, nullptr) != pdPASS) {
      delete request; httpProbeBusy = false; Serial.println("ERROR HTTP TASK");
    }
  }
  else if (line == "WEBTEST" && !webTestBusy) {
    webTestBusy = true;
    if (xTaskCreate(webTestTask, "webtest", 6144, nullptr, 1, nullptr) != pdPASS) {
      webTestBusy = false; Serial.println("ERROR WEBTEST TASK");
    }
  }
  else if (line == "NEXT") { advancePhoto(1); Serial.println("OK NEXT"); }
  else if (line == "PAUSE") { paused = !paused; Serial.println(paused ? "OK PAUSED" : "OK PLAYING"); }
  else if (line == "RESTART") { Serial.println("OK RESTART"); Serial.flush(); ESP.restart(); }
  else if (line.startsWith("TIME ")) {
    time_t epoch = strtoll(line.c_str() + 5, nullptr, 10);
    if (epoch > 1700000000 && epoch < 4102444800LL) { timeval value{epoch, 0}; settimeofday(&value, nullptr); headerDirty = true; Serial.println("OK TIME"); }
    else Serial.println("ERROR TIME");
  } else if (line.startsWith("PHOTO ")) {
    int slot; unsigned long bytes, hash;
    if (sscanf(line.c_str(), "PHOTO %d %lu %lx", &slot, &bytes, &hash) != 3 || bytes < 8 || bytes > MAX_PHOTO_BYTES) { Serial.println("ERROR PHOTO HEADER"); return; }
    if (!beginUpload(slot)) { Serial.println("ERROR " + uploadError); return; }
    serialTargetBytes=bytes;serialExpectedHash = hash; serialReceiving = true; serialLastByte = millis(); Serial.println("READY PHOTO");
  } else if (line.startsWith("DELETE ")) {
    int slot = line.substring(7).toInt();
    if (slot >= 0 && slot < photoLimit() && !uploading && fsReady) {
      if(deleteStoredPhoto(slot)){if(slot==currentPhoto)advancePhoto(1);Serial.println("OK DELETE");}else Serial.println("ERROR DELETE");
    }
  }
}
void serviceSerial() {
  if (serialReceiving) {
    uint8_t buffer[1024];
    size_t count = min(static_cast<size_t>(Serial.available()), min(sizeof(buffer), serialTargetBytes - uploadBytes));
    if (count) {
      size_t got = Serial.read(buffer, count); appendUpload(buffer, got); serialLastByte = millis();
      if (!uploadError.isEmpty()) { finishUpload(true); serialReceiving = false; Serial.println("ERROR PHOTO WRITE"); }
      else if (uploadBytes == serialTargetBytes) {
        if (uploadHash != serialExpectedHash) uploadError = "Checksum uyusmuyor.";
        bool ok = finishUpload(); serialReceiving = false;
        Serial.println(ok ? "OK PHOTO" : "ERROR " + uploadError);
      }
    } else if (millis() - serialLastByte > 15000) { finishUpload(true); serialReceiving = false; Serial.println("ERROR PHOTO TIMEOUT"); }
    return;
  }
  for (int i = 0; i < 128 && Serial.available(); i++) {
    char c = Serial.read();
    if (c == '\n') { serialLine.trim(); processSerialLine(serialLine); serialLine = ""; if (serialReceiving) break; }
    else if (c != '\r') { if (serialLine.length() < 12288) serialLine += c; else serialLine = ""; }
  }
}
void setup() {
  Serial.setRxBufferSize(8192); Serial.begin(460800); delay(100);
  prefs.begin("fotocerceve", false);
  ssid = prefs.getString("ssid", ""); wifiPassword = prefs.getString("pass", "");
  city = prefs.getString("city", ""); latitude = prefs.getDouble("lat", 0); longitude = prefs.getDouble("lon", 0);
  weatherConfigured = prefs.getBool("weather", false);
  if (!city.length()) weatherConfigured = false;
  landscape=prefs.getBool("landscape",false);fillPhoto=prefs.getBool("fill",true);
  barPosition=parseBarPosition(prefs.getString("bar","top"));
  clockColor=min<uint8_t>(5,prefs.getUChar("clockCol",0));dateColor=min<uint8_t>(5,prefs.getUChar("dateCol",0));weatherColor=min<uint8_t>(5,prefs.getUChar("weatherCol",0));
  pictureBrightness=constrain(prefs.getUInt("brightness",100),20u,150u);pictureContrast=constrain(prefs.getUInt("contrast",100),50u,150u);
  slideSeconds = constrain(prefs.getUInt("interval", 20), 5u, 3600u);
  fsReady = LittleFS.begin(false);
  // Format only once for this new application, never automatically after corruption.
  if (!fsReady && !prefs.getBool("fsinit", false)) { if (LittleFS.format()) fsReady = LittleFS.begin(false); }
  if (fsReady) prefs.putBool("fsinit", true);
  // SD uses a separate bus so existing TFT wiring stays as installed.
  pinMode(TFT_CS_PIN,OUTPUT);digitalWrite(TFT_CS_PIN,HIGH);
  pinMode(SD_CS_PIN,OUTPUT);digitalWrite(SD_CS_PIN,HIGH);
  SPI.begin(TFT_SCK_PIN,TFT_MISO_PIN,TFT_MOSI_PIN,TFT_CS_PIN);
  sdSPI.begin(SD_SCK_PIN,SD_MISO_PIN,SD_MOSI_PIN,SD_CS_PIN);
  if(SD.begin(SD_CS_PIN,sdSPI,4000000,"/sd",8,false)){
    usingSD=true;photoFS=&SD;fsReady=true;
  }else{SD.end();digitalWrite(SD_CS_PIN,HIGH);storageError="SD kart algilanmadi. Dahili album kullaniliyor.";}
  scanPhotos(); advancePhoto(1);
  setenv("TZ", "TRT-3", 1); tzset();
  WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t info) {
    if (event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED) wifiReason=info.wifi_sta_disconnected.reason;
    if (event==ARDUINO_EVENT_WIFI_STA_GOT_IP) wifiReason=0;
  });
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false); WiFi.mode(WIFI_AP_STA); WiFi.setSleep(false);
  apName=AP_NAME;
  WiFi.softAP(apName.c_str(), AP_PASSWORD);
  if (ssid.length()) WiFi.begin(ssid.c_str(), wifiPassword.c_str());
  sntp_set_time_sync_notification_cb([](struct timeval *){internetTimeSynced=true;});
  configTime(3 * 3600, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  lastTimeAttempt=millis();
  tft.begin(TFT_SPEED); tft.setRotation(landscape?1:0);
  tft.fillScreen(ILI9341_BLACK); tft.setTextWrap(false);
  geoQueue=xQueueCreate(1,sizeof(String*));
  weatherQueue = xQueueCreate(1, sizeof(Weather));
  setupRoutes(); renderHeader(); renderPhoto();
  Serial.println("FOTO_CERCEVE_READY"); Serial.println(statusJSON());
}
void loop() {
  serviceSerial();
  if (!serialReceiving) server.handleClient();
  if (connectPending && !uploading && !wifiScanning && !wifiScanPending && static_cast<int32_t>(millis()-connectAfter)>=0) {
    connectPending = false; WiFi.disconnect();
    if (ssid.length()) WiFi.begin(ssid.c_str(), wifiPassword.c_str());
    nextWeather = 0;
  }
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != connectionKnown) { connectionKnown = connected; headerDirty = true; }
  if (ssid.length() && !connected && !uploading && !wifiScanning && !wifiScanPending && !connectPending && millis() - lastReconnect > 30000) { lastReconnect = millis(); WiFi.reconnect(); }
  if(connected&&!internetTimeSynced&&!uploading&&millis()-lastTimeAttempt>45000){
    configTime(3*3600,0,"pool.ntp.org","time.cloudflare.com","time.google.com");lastTimeAttempt=millis();
  }
  serviceGeo();
  if (weatherQueue) serviceWeather();
  if (!paused && !uploading && photoCount() > 1 && millis() - lastSlide >= slideSeconds * 1000UL) advancePhoto(1);
  if (!uploading && (headerDirty || millis() - lastHeader > 1000)) { renderHeader(); headerDirty = false; lastHeader = millis(); }
  if (photoDirty && !uploading) renderPhoto();
  delay(1);
}
