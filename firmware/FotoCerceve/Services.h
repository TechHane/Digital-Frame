#pragma once
#include <memory>
String base64Text(const uint8_t *bytes,size_t length) {
  size_t capacity=4*((length+2)/3)+1,used=0;
  std::unique_ptr<unsigned char[]> output(new unsigned char[capacity]);
  if (mbedtls_base64_encode(output.get(),capacity,&used,bytes,length)!=0) return "";
  output[used]=0;return String(reinterpret_cast<char*>(output.get()));
}
void sendPhotoChunk(int slot,bool thumbnail) {
  Photo p=photoInfo(slot);
  if(!p.present){sendError(404,"Fotograf okunamadi.");return;}
  if (server.hasArg("hash")) {
    char hash[9];snprintf(hash,sizeof(hash),"%08lx",static_cast<unsigned long>(p.hash));
    if (server.arg("hash")!=hash) { sendError(409,"Fotograf degisti. Onizleme yenilenecek.");return; }
  }
  int w=p.cropWidth,h=p.cropHeight;
  if (w>=h) { h=max(1,h*64/w);w=64; } else { w=max(1,w*64/h);h=64; }
  uint32_t total=thumbnail?w*h*2:p.bytes;
  long offset=server.hasArg("offset")?server.arg("offset").toInt():0;
  if (offset<0 || static_cast<uint32_t>(offset)>=total || offset%2) { sendError(400,"Fotograf parcasi gecersiz.");return; }
  size_t count=min(static_cast<size_t>(3072),static_cast<size_t>(total-offset));
  uint8_t bytes[3072];File f=photoFS->open(pathFor(slot),"r");
  if (!f) { sendError(404,"Fotograf acilamadi.");return; }
  if (!thumbnail) {
    f.seek(offset);if (f.read(bytes,count)!=count) { sendError(500,"Fotograf okunamadi.");return; }
  } else {
    uint16_t row[MAX_SOURCE_EDGE];int previous=-1;
    for(size_t i=0;i<count;i+=2) {
      int pixel=(offset+i)/2,sy=p.top+(pixel/w)*p.cropHeight/h;
      if(sy!=previous) {
        f.seek(p.offset+sy*p.width*2);
        if (f.read(reinterpret_cast<uint8_t*>(row),p.width*2)!=p.width*2) { sendError(500,"Onizleme okunamadi.");return; }
        previous=sy;
      }
      uint16_t color=row[p.left+(pixel%w)*p.cropWidth/w];bytes[i]=color&255;bytes[i+1]=color>>8;
    }
  }
  f.close(); // Never keep a filesystem handle open while the phone receives data.
  cJSON *result=cJSON_CreateObject();
  cJSON_AddNumberToObject(result,"offset",offset);cJSON_AddNumberToObject(result,"next",offset+count);
  cJSON_AddNumberToObject(result,"total",total);cJSON_AddNumberToObject(result,"width",w);cJSON_AddNumberToObject(result,"height",h);
  cJSON_AddStringToObject(result,"data",base64Text(bytes,count).c_str());
  server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",jsonString(result));
}
void startWifiScan() {
  if (!localMutation()) return;
  if (!wifiScanning && !wifiScanPending) {
    // Send the HTTP acknowledgement before changing the radio channel.
    wifiScanPending=true;wifiScanError=false;wifiScanAt=millis()+800;
  }
  server.send(200,"application/json","{\"state\":\"scanning\"}");
}
void sendWifiScan() {
  int count=WiFi.scanComplete();
  if (wifiScanPending || count==WIFI_SCAN_RUNNING) { server.send(200,"application/json","{\"state\":\"scanning\"}");return; }
  if (wifiScanError) { server.send(200,"application/json","{\"state\":\"error\",\"error\":\"Ag taramasi baslatilamadi. Tekrar dene.\"}");return; }
  wifiScanning=false;
  if (count<0) { server.send(200,"application/json","{\"state\":\"idle\",\"networks\":[]}");return; }
  cJSON *root=cJSON_CreateObject();cJSON_AddStringToObject(root,"state","ready");cJSON *list=cJSON_AddArrayToObject(root,"networks");
  // Scan results are already in RSSI order; remove duplicate access points.
  String seen[20];int used=0;
  for(int i=0;i<count && used<20;i++) {
    String name=WiFi.SSID(i);if(!name.length())continue;
    bool duplicate=false;for(int j=0;j<used;j++)if(seen[j]==name)duplicate=true;
    if(duplicate)continue;seen[used++]=name;
    cJSON *network=cJSON_CreateObject();cJSON_AddStringToObject(network,"ssid",name.c_str());
    cJSON_AddNumberToObject(network,"rssi",WiFi.RSSI(i));cJSON_AddBoolToObject(network,"open",WiFi.encryptionType(i)==WIFI_AUTH_OPEN);
    cJSON_AddItemToArray(list,network);
  }
  server.send(200,"application/json; charset=utf-8",jsonString(root));
}
String encodeQuery(const String &value) {
  String result;char encoded[4];
  for(size_t i=0;i<value.length();i++) {
    unsigned char c=value[i];
    if(isalnum(c) || c=='-' || c=='_' || c=='.')result+=static_cast<char>(c);
    else { snprintf(encoded,sizeof(encoded),"%%%02X",c);result+=encoded; }
  }
  return result;
}
void geoWork(void *data) {
  String query=*static_cast<String*>(data);delete static_cast<String*>(data);
  String url="https://geocoding-api.open-meteo.com/v1/search?name="+encodeQuery(query)+"&count=6&language=tr&format=json";
  String body;esp_http_client_config_t config{};config.url=url.c_str();config.crt_bundle_attach=esp_crt_bundle_attach;
  config.timeout_ms=10000;config.event_handler=weatherEvent;config.user_data=&body;
  cJSON *output=cJSON_CreateObject();cJSON_AddNumberToObject(output,"job",geoJobId);
  esp_http_client_handle_t client=esp_http_client_init(&config);bool success=false;
  if(client) {
    if(esp_http_client_perform(client)==ESP_OK && esp_http_client_get_status_code(client)==200) {
      cJSON *root=cJSON_Parse(body.c_str());cJSON *places=cJSON_GetObjectItemCaseSensitive(root,"results");
      if(root) {
        cJSON_AddStringToObject(output,"state","ready");cJSON *list=cJSON_AddArrayToObject(output,"results");
        cJSON *place=nullptr;cJSON_ArrayForEach(place,places) {
          cJSON *name=cJSON_GetObjectItemCaseSensitive(place,"name"),*lat=cJSON_GetObjectItemCaseSensitive(place,"latitude"),*lon=cJSON_GetObjectItemCaseSensitive(place,"longitude");
          if(!cJSON_IsString(name)||!cJSON_IsNumber(lat)||!cJSON_IsNumber(lon))continue;
          cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"name",name->valuestring);
          cJSON_AddNumberToObject(item,"latitude",lat->valuedouble);cJSON_AddNumberToObject(item,"longitude",lon->valuedouble);
          for(const char *field:{"admin1","country"}) { cJSON *value=cJSON_GetObjectItemCaseSensitive(place,field);if(cJSON_IsString(value))cJSON_AddStringToObject(item,field,value->valuestring); }
          cJSON_AddItemToArray(list,item);
        }
        success=true;
      }
      cJSON_Delete(root);
    }
    esp_http_client_cleanup(client);
  }
  if(!success) { cJSON_AddStringToObject(output,"state","error");cJSON_AddStringToObject(output,"error","Konum servisine ulasilamadi. Internet baglantisini kontrol et."); }
  String *result=new String(jsonString(output));if(xQueueSend(geoQueue,&result,0)!=pdTRUE)delete result;
}
void geoTask(void *data) { geoWork(data);vTaskDelete(nullptr); }
void startGeoSearch() {
  if(!localMutation())return;
  String query=server.arg("query");query.trim();
  if(query.length()<2 || query.length()>100) { sendError(400,"Il veya ilce adini yaz.");return; }
  if(WiFi.status()!=WL_CONNECTED) { sendError(409,"Konum aramasi icin once cerceveyi ev Wi-Fi agina bagla.");return; }
  if(geoBusy || weatherBusy) { sendError(503,"Hava durumu guncelleniyor. Biraz sonra tekrar dene.");return; }
  geoJobId++;geoBusy=true;geoResult="{\"state\":\"searching\",\"job\":"+String(geoJobId)+"}";
  String *job=new String(query);
  if(xTaskCreate(geoTask,"geo",8192,job,1,nullptr)!=pdPASS) { delete job;geoBusy=false;sendError(503,"Arama baslatilamadi.");return; }
  server.send(200,"application/json",geoResult);
}
void serviceGeo() {
  if(wifiScanPending && !uploading && static_cast<int32_t>(millis()-wifiScanAt)>=0) {
    wifiScanPending=false;
    if(WiFi.status()!=WL_CONNECTED) { WiFi.disconnect(false,false);lastReconnect=millis(); }
    WiFi.scanDelete();int result=WiFi.scanNetworks(true,false,false,80);
    wifiScanning=result==WIFI_SCAN_RUNNING;wifiScanError=result==WIFI_SCAN_FAILED;
  }
  if(wifiScanning && WiFi.scanComplete()!=WIFI_SCAN_RUNNING)wifiScanning=false;
  String *result=nullptr;
  if(geoQueue && xQueueReceive(geoQueue,&result,0)==pdTRUE) { geoResult=*result;delete result;geoBusy=false; }
}
