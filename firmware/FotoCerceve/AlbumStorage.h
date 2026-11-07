#pragma once
// The SD index stays on the card; only its presence bitmap occupies RAM.
// Each bucket contains at most 1000 short (8.3) filenames.
constexpr uint32_t INDEX_MAGIC=0x33414854, RECORD_MAGIC=0x33504854;
struct __attribute__((packed)) AlbumRecord {
  uint32_t magic,hash,bytes;
  uint16_t width,height,offset,left,top,cropWidth,cropHeight,flags;
  uint32_t checksum;
};
static_assert(sizeof(AlbumRecord)==32,"Stable SD catalog format");
uint8_t photoBits[(SD_PHOTO_LIMIT+7)/8]{};
uint32_t albumCount=0;
Photo internalPhotos[INTERNAL_PHOTO_LIMIT];
struct PhotoInfoCacheEntry { int slot=-1;Photo photo{}; } photoInfoCache[32];
uint8_t photoInfoCacheNext=0;
uint32_t sdIndexReads=0;
String storageError;
uint64_t storageTotal=0,storageUsed=0;
constexpr const char *INDEX_PATH="/TechHane/album.idx";
constexpr const char *JOURNAL_PATH="/TechHane/pending.bin";

int photoLimit(){return usingSD?SD_PHOTO_LIMIT:INTERNAL_PHOTO_LIMIT;}
bool photoPresent(int slot){return slot>=0&&slot<photoLimit()&&(usingSD?(photoBits[slot/8]&(1u<<(slot%8))):internalPhotos[slot].present);}
void markPresent(int slot,bool present){
  bool old=(photoBits[slot/8]&(1u<<(slot%8)))!=0;
  if(old!=present){if(present)albumCount++;else albumCount--;}
  if(present)photoBits[slot/8]|=1u<<(slot%8);else photoBits[slot/8]&=~(1u<<(slot%8));
}
bool recordValid(const AlbumRecord &record){
  return record.magic==RECORD_MAGIC && record.checksum==hashBytes(2166136261u,reinterpret_cast<const uint8_t*>(&record),28);
}
Photo recordPhoto(const AlbumRecord &record){
  Photo p{};if(!recordValid(record)||!record.flags)return p;
  p.present=true;p.hash=record.hash;p.bytes=record.bytes;p.width=record.width;p.height=record.height;
  p.offset=record.offset;p.left=record.left;p.top=record.top;p.cropWidth=record.cropWidth;p.cropHeight=record.cropHeight;
  if(record.flags!=1||!p.width||!p.height||p.width>MAX_SOURCE_EDGE||p.height>MAX_SOURCE_EDGE||p.width*p.height>MAX_SOURCE_PIXELS||!p.cropWidth||!p.cropHeight||p.left+p.cropWidth>p.width||p.top+p.cropHeight>p.height||p.bytes>MAX_PHOTO_BYTES||(p.offset!=0&&p.offset!=8)||p.bytes!=p.offset+p.width*p.height*2u)return {};
  return p;
}
Photo photoInfo(int slot){
  if(!photoPresent(slot))return {};
  if(!usingSD)return internalPhotos[slot];
  for(const auto &entry:photoInfoCache)if(entry.slot==slot)return entry.photo;
  sdIndexReads++;
  File f=photoFS->open(INDEX_PATH,"r");AlbumRecord record{};
  if(!f||!f.seek(32u+slot*32u)||f.read(reinterpret_cast<uint8_t*>(&record),32)!=32){storageError="SD kart okunamadi. Karti ve baglantilari kontrol et.";return {};}
  Photo p=recordPhoto(record);
  if(p.present){photoInfoCache[photoInfoCacheNext].slot=slot;photoInfoCache[photoInfoCacheNext].photo=p;photoInfoCacheNext=(photoInfoCacheNext+1)%32;}
  return p;
}
bool setPhotoInfo(int slot,const Photo &p){
  if(slot<0||slot>=photoLimit())return false;
  if(!usingSD){internalPhotos[slot]=p;return true;}
  for(auto &entry:photoInfoCache)if(entry.slot==slot)entry.slot=-1;
  AlbumRecord record{};record.magic=RECORD_MAGIC;record.hash=p.hash;record.bytes=p.bytes;
  record.width=p.width;record.height=p.height;record.offset=p.offset;record.left=p.left;record.top=p.top;
  record.cropWidth=p.cropWidth;record.cropHeight=p.cropHeight;record.flags=p.present?1:0;
  record.checksum=hashBytes(2166136261u,reinterpret_cast<uint8_t*>(&record),28);
  File f=photoFS->open(INDEX_PATH,"r+");
  if(!f||!f.seek(32u+slot*32u)||f.write(reinterpret_cast<uint8_t*>(&record),32)!=32){storageError="SD album dizini kaydedilemedi.";return false;}
  f.flush();f.close();markPresent(slot,p.present);return true;
}
int photoCount(){if(usingSD)return albumCount;int count=0;for(const auto &p:internalPhotos)if(p.present)count++;return count;}
int nextFreeSlot(){for(int i=0;i<photoLimit();i++)if(!photoPresent(i))return i;return -1;}
void refreshStorageUsage(){
  if(usingSD){storageTotal=SD.totalBytes();storageUsed=SD.usedBytes();}
  else{storageTotal=LittleFS.totalBytes();storageUsed=LittleFS.usedBytes();}
}
bool preparePhotoFolder(int slot){
  if(!usingSD)return true;
  char path[24];snprintf(path,sizeof(path),"/TechHane/%02d",slot/1000);
  return photoFS->exists(path)||photoFS->mkdir(path);
}
bool startStorageJournal(int slot,bool deleting=false){
  if(!usingSD)return true;
  uint32_t journal[]={INDEX_MAGIC,static_cast<uint32_t>(slot),deleting?1u:0u,0};
  journal[3]=hashBytes(2166136261u,reinterpret_cast<uint8_t*>(journal),12);
  File f=photoFS->open(JOURNAL_PATH,"w");if(!f||f.write(reinterpret_cast<uint8_t*>(journal),16)!=16)return false;
  f.flush();return true;
}
void finishStorageJournal(){if(usingSD)photoFS->remove(JOURNAL_PATH);}
bool recoverSDJournal(){
  if(!photoFS->exists(JOURNAL_PATH))return true;
  File f=photoFS->open(JOURNAL_PATH,"r");uint32_t journal[4]{};
  bool valid=f&&f.read(reinterpret_cast<uint8_t*>(journal),16)==16&&journal[0]==INDEX_MAGIC&&journal[1]<SD_PHOTO_LIMIT&&journal[3]==hashBytes(2166136261u,reinterpret_cast<uint8_t*>(journal),12);f.close();
  if(!valid){storageError="SD kurtarma kaydi okunamadi. Karttaki dosyalar korundu.";return false;}
  int slot=journal[1];String path=pathFor(slot),backup=pathFor(slot,".bak");
  if(journal[2]){
    if(photoFS->exists(path)&&!photoFS->remove(path))return false;
    if(!setPhotoInfo(slot,{}))return false;
    photoFS->remove(backup);
  }else{
    Photo p{};
    if(!readPhotoInfo(path,p)){
      if(!readPhotoInfo(backup,p)){
        if(photoFS->exists(path)||photoFS->exists(backup)){
          storageError="SD fotografi kurtarma sirasinda okunamadi; dosyalar korundu.";return false;
        }
        p={};
      }
      else{photoFS->remove(path);if(!photoFS->rename(backup,path))return false;}
    }
    if(!setPhotoInfo(slot,p))return false;
    photoFS->remove(backup);
  }
  photoFS->remove(pathFor(slot,".tmp"));finishStorageJournal();return true;
}
bool loadSDCatalog(){
  for(auto &entry:photoInfoCache)entry.slot=-1;
  memset(photoBits,0,sizeof(photoBits));albumCount=0;
  if(!photoFS->exists("/TechHane")&&!photoFS->mkdir("/TechHane")){storageError="SD kartta album klasoru olusturulamadi.";return false;}
  if(!photoFS->exists(INDEX_PATH)){
    // Never treat an unindexed existing album as empty and overwrite its files.
    for(int i=0;i<66;i++){char folder[24];snprintf(folder,sizeof(folder),"/TechHane/%02d",i);if(photoFS->exists(folder)){storageError="SD album dizini eksik; mevcut dosyalar korundu.";return false;}}
    uint32_t header[8]={INDEX_MAGIC,1,32,SD_PHOTO_LIMIT,0,0,0,0};
    File f=photoFS->open(INDEX_PATH,"w");if(!f||f.write(reinterpret_cast<uint8_t*>(header),32)!=32)return false;f.flush();
  }
  File f=photoFS->open(INDEX_PATH,"r");uint32_t header[8]{};
  if(!f||f.read(reinterpret_cast<uint8_t*>(header),32)!=32||header[0]!=INDEX_MAGIC||header[1]!=1||header[2]!=32||header[3]!=SD_PHOTO_LIMIT){storageError="SD album dizini taninamadi; kart bicimlendirilmedi.";return false;}
  f.close();if(!recoverSDJournal())return false;
  memset(photoBits,0,sizeof(photoBits));albumCount=0;
  f=photoFS->open(INDEX_PATH,"r");if(!f||!f.seek(32))return false;
  // Sequential reads keep startup proportional to metadata, not photo bytes.
  AlbumRecord block[16];int slot=0;
  while(f.available()&&slot<SD_PHOTO_LIMIT){
    size_t bytes=f.read(reinterpret_cast<uint8_t*>(block),sizeof(block));
    if(!bytes||bytes%32){storageError="SD album dizini eksik yazilmis.";f.close();return false;}
    for(size_t i=0;i<bytes/32;i++,slot++){
      if(!recordValid(block[i])){storageError="SD album dizini hasarli; dosyalar korundu.";f.close();return false;}
      Photo p=recordPhoto(block[i]);
      if(block[i].flags&&!p.present){storageError="SD fotograf bilgisi hasarli; dosyalar korundu.";f.close();return false;}
      if(p.present)markPresent(slot,true);
    }
    yield();
  }
  f.close();refreshStorageUsage();return true;
}
bool deleteStoredPhoto(int slot){
  if(usingSD&&photoFS->exists(JOURNAL_PATH))return false;
  if(!photoPresent(slot))return true;
  if(!startStorageJournal(slot,true))return false;
  if(!photoFS->remove(pathFor(slot)))return false;
  if(!setPhotoInfo(slot,{}))return false;
  finishStorageJournal();refreshStorageUsage();return true;
}
