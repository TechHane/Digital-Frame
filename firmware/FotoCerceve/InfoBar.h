#pragma once
#include "BarAssets.h"

constexpr const char *COLOR_NAMES[]={"white","yellow","cyan","green","orange","pink"};
constexpr uint32_t COLOR_RGB[]={0xFFFFFF,0xFFDB5A,0x73DFFF,0x8FE3A8,0xFFAD66,0xFF9ED0};
inline int colorIndex(const String &name){for(int i=0;i<6;i++)if(name==COLOR_NAMES[i])return i;return -1;}
inline uint16_t color565(uint8_t index){uint32_t rgb=COLOR_RGB[index<6?index:0];return ((rgb>>8)&0xF800)|((rgb>>5)&0x07E0)|((rgb>>3)&31);}
enum class BarPosition : uint8_t { Top, Bottom, Hidden };
inline const char *barName(BarPosition position) {
  return position==BarPosition::Bottom?"bottom":position==BarPosition::Hidden?"hidden":"top";
}
inline BarPosition parseBarPosition(const String &value) {
  return value=="bottom"?BarPosition::Bottom:value=="hidden"?BarPosition::Hidden:BarPosition::Top;
}
inline int weatherIcon(int code, bool day, bool valid) {
  if (!valid) return 0;
  if (code==0 || code==1) return day?1:2;
  if (code==2) return day?3:4;
  if (code==3) return 5;
  if (code==45 || code==48) return 8;
  if ((code>=51 && code<=67) || (code>=80 && code<=82)) return 6;
  if ((code>=71 && code<=77) || code==85 || code==86) return 7;
  if (code>=95 && code<=99) return 9;
  return 0;
}
inline const char *conditionName(int icon) {
  switch(icon) {
    case 1:case 2:return "Açık";
    case 3:case 4:return "Parçalı bulutlu";
    case 5:return "Bulutlu";
    case 6:return "Yağmurlu";
    case 7:return "Karlı";
    case 8:return "Sisli";
    case 9:return "Gök gürültülü";
    default:return "Hava durumu bekleniyor";
  }
}
// Smoothed 16 px Noto Sans digits; identical raster assets in the phone preview.
inline int glyphIndex(char c){const char *p=strchr(BAR_CHARS,c);return p?p-BAR_CHARS:13;}
inline int barTextWidth(const String &s){int w=0;for(size_t i=0;i<s.length();i++)w+=BAR_WIDTHS[glyphIndex(s[i])];return w;}
inline uint16_t blendBar(uint16_t color,int a){
  int r=(((color>>11)&31)*a+1*(255-a)+127)/255;
  int g=(((color>>5)&63)*a+2*(255-a)+127)/255;
  int b=((color&31)*a+2*(255-a)+127)/255;return (r<<11)|(g<<5)|b;
}
inline void barText(Adafruit_GFX &canvas,const String &s,int x,uint16_t color){
  for(size_t i=0;i<s.length();i++){int index=glyphIndex(s[i]),w=BAR_WIDTHS[index];
    for(int y=0;y<18;y++)for(int col=0;col<w;col++){int a=pgm_read_byte(&BAR_GLYPHS[index][y*w+col]);if(a)canvas.drawPixel(x+col,y,blendBar(color,a));}x+=w;
  }
}
inline void drawInfoBar(Adafruit_GFX &canvas,int width,const char *clockText,const char *dateText,const String &temperature,int icon,uint16_t clockColor,uint16_t dateColor,uint16_t weatherColor){
  canvas.fillScreen(0x0842);
  int clockEnd=3+barTextWidth(clockText),temperatureX=width-3-6-barTextWidth(temperature),iconX=temperatureX-19;
  int dateX=(clockEnd+iconX-barTextWidth(dateText))/2;
  barText(canvas,clockText,3,clockColor);barText(canvas,dateText,dateX,dateColor);
  for(int y=0;y<16;y++)for(int x=0;x<16;x++)if(pgm_read_byte(&BAR_ICONS[icon][y*16+x]))canvas.drawPixel(iconX+x,y+1,weatherColor);
  barText(canvas,temperature,temperatureX,weatherColor);canvas.drawCircle(width-6,4,2,weatherColor);
}
