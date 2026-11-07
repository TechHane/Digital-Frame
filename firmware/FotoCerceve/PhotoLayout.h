#pragma once
struct Photo {
  bool present = false;
  uint32_t hash = 0, bytes = 0;
  uint16_t width = 0, height = 0, offset = 0;
  uint16_t left = 0, top = 0, cropWidth = 0, cropHeight = 0;
};
struct PhotoView { int sx, sy, sw, sh, dx, dy, dw, dh; };
inline PhotoView photoView(const Photo &p, int width, int height, bool cover) {
  PhotoView v{p.left,p.top,p.cropWidth,p.cropHeight,0,0,width,height};
  if (cover) {
    if (v.sw * height > v.sh * width) { int next = max(1,v.sh * width / height); v.sx += (v.sw-next)/2; v.sw=next; }
    else { int next = max(1,v.sw * height / width); v.sy += (v.sh-next)/2; v.sh=next; }
  } else {
    if (v.sw * height > v.sh * width) { v.dh=max(1,v.sh * width / v.sw); v.dy=(height-v.dh)/2; }
    else { v.dw=max(1,v.sw * height / v.sh); v.dx=(width-v.dw)/2; }
  }
  return v;
}
