#pragma once
#include <chrono>
namespace octaryn::client::app {
// Native main entry excludes OS process creation, loader and static initialization.
inline std::chrono::steady_clock::time_point app_main_entry;
inline void start_app_clock() {app_main_entry=std::chrono::steady_clock::now();}
inline double app_elapsed_ms() {
  if(app_main_entry==std::chrono::steady_clock::time_point{})return -1;
  return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-app_main_entry).count();
}
}
