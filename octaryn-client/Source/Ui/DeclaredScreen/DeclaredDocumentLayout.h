#pragma once
#include <algorithm>
#include <string_view>

namespace octaryn::client::ui {
struct DocumentFit {double scale{},left{},top{},width{},height{};};
inline bool document_fit_mode_valid(std::string_view mode) {return mode=="contain" || mode=="height";}
inline bool document_canvas_valid(unsigned width,unsigned height) {
  return (width==0 && height==0) || (width>=64 && height>=64 && width<=4096 && height<=4096);
}
inline bool document_fit(unsigned logical_width,unsigned logical_height,int width,int height,DocumentFit& output,
    std::string_view mode="contain") {
  if(!document_fit_mode_valid(mode) || !document_canvas_valid(logical_width,logical_height) || logical_width==0 || width<1 || height<1)return false;
  const double scale=mode=="height"?double(height)/logical_height:
      std::min(double(width)/logical_width,double(height)/logical_height);
  const double fitted_width=mode=="height"?width/scale:logical_width;
  output={scale,mode=="height"?0:(width-fitted_width*scale)/2,mode=="height"?0:(height-logical_height*scale)/2,
      fitted_width,double(logical_height)};return true;
}
}
