#pragma once
#include <json.hpp>
#include <cmath>
namespace battery_status {
inline int flag(const nlohmann::json& readings,const char* key){
    auto value=readings.find(key);
    if(value==readings.end())return -1;
    if(value->is_boolean())return value->get<bool>()?1:0;
    if(value->is_number()){double n=value->get<double>();if(std::isfinite(n)&&(n==0||n==1))return int(n);}
    return -1;
}
// Missing/invalid/stale data are unknown. Never infer a cable from charge level.
inline int connected(const nlohmann::json& readings,bool fresh){
    if(!fresh)return -1;
    int plugged=flag(readings,"battery_plugged");
    if(plugged>=0)return plugged;
    return flag(readings,"battery_charging")==1?1:-1;
}
}
