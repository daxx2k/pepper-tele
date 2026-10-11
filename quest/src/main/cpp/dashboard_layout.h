#pragma once
#include <algorithm>
#include <cmath>
namespace dashboard_layout {
struct Rect {float x,y,w,h;};
inline Rect contain(Rect bounds,int sourceWidth,int sourceHeight){if(sourceWidth<=0||sourceHeight<=0)return bounds;float scale=std::min(bounds.w/sourceWidth,bounds.h/sourceHeight),w=sourceWidth*scale,h=sourceHeight*scale;return {bounds.x+(bounds.w-w)*.5f,bounds.y+(bounds.h-h)*.5f,w,h};}
// Left diagnostics, central fused vision, right stacked operator controls.
// Semantic group indices remain stable; View and Pose offsets are retired.
constexpr float panelGap=12;
constexpr float leftX=20,leftWidth=440,centreX=leftX+leftWidth+panelGap,rightX=960;
constexpr float centreWidth=rightX-centreX-panelGap,rowTop=56+panelGap,bodyBottom=950;
constexpr Rect header{20,8,1360,48};
inline float helpOffsetWidthFraction(float helpWidth,float scale,float canvasWidth){return .5f+.5f*helpWidth/canvasWidth*scale+(panelGap-20-12*scale)/canvasWidth;}
constexpr float motionHeight=276;
constexpr float voiceY=rowTop+motionHeight+panelGap,appearanceY=voiceY+292+panelGap,lowerY=appearanceY+134+panelGap;
constexpr float temperatureY=rowTop+451+panelGap,connectionY=temperatureY+249+panelGap;

constexpr float x[]={rightX,rightX,rightX,0,leftX,0,rightX};
constexpr float width[]={420,420,420,0,leftWidth,0,420};
constexpr float groupY[]={rowTop,voiceY,lowerY,0,connectionY,0,appearanceY};
constexpr float groupHeight[]={276,292,144,0,158,0,134};
constexpr Rect fusion{centreX,rowTop,centreWidth,lowerY-panelGap-rowTop};
constexpr Rect fusionImage{488,104,444,588};
constexpr Rect depthCard{centreX,806,156,144},lidarCard{640,806,144,144};
constexpr Rect tablet{796,806,152,144};
// Shared title band and safe inset, including the rounded lower corners.
constexpr Rect mediaContent(Rect card){return {card.x+12,card.y+36,card.w-24,card.h-48};}
constexpr Rect depthContent=mediaContent(depthCard),lidar=mediaContent(lidarCard),tabletContent=mediaContent(tablet);
constexpr Rect cameras[]={{488,rowTop+38,444,333},{488,rowTop+38+333+panelGap,444,333},depthContent};
constexpr float cameraGap=panelGap,cameraBlendPixels=8,cameraCornerRadius=8;
// Pepper tablet keeps its native landscape 16:10 aspect ratio.
constexpr Rect tabletScreen{tabletContent.x,tabletContent.y,tabletContent.w,tabletContent.w*10/16};
constexpr Rect comparison{leftX,rowTop,leftWidth,451};
constexpr Rect temperatures{leftX,temperatureY,leftWidth,249};
constexpr Rect temperatureUnit{temperatures.x+temperatures.w-80,temperatures.y+4,70,24};
inline float displayTemperature(float celsius,bool fahrenheit){return fahrenheit?celsius*1.8f+32.f:celsius;}
inline Rect voiceButton(bool phrase,int ordinal,int count){int rows=std::max(1,(count+1)/2);float base=phrase?157.f:33.f,space=phrase?127.f:102.f,h=std::min(phrase?44.f:34.f,space/rows),w=(width[1]-22)/2;return {x[1]+8+(ordinal%2)*(w+6),groupY[1]+base+(ordinal/2)*h,w,h-6};}
constexpr float temperatureStrideY=50,temperatureTileHeight=46;
inline Rect temperatureTile(int index){return {temperatures.x+10+(index%5)*84,temperatures.y+31+(index/5)*temperatureStrideY,80,temperatureTileHeight};}
constexpr Rect headerLayout{1250,6,48,48},headerHelp{1306,6,48,48};
constexpr Rect statusBar{20,bodyBottom+panelGap,1360,40};
constexpr Rect windowMove{550,statusBar.y+statusBar.h+panelGap,240,40},windowPin{802,statusBar.y+statusBar.h+panelGap,40,40};
inline bool windowHover(float x,float y){return x>=530&&x<=866&&y>=995&&y<=1060;}
inline float grabScale(float initial,float depth){return std::max(.65f,std::min(1.f,initial*std::exp(-depth*1.5f)));}
constexpr Rect clearLaser{lidarCard.x+lidarCard.w-52,lidarCard.y+lidarCard.h-38,44,30};
inline int temperatureAt(float px,float py){float dx=px-temperatures.x-10,dy=py-temperatures.y-31;if(dx<0||dy<0)return -1;int col=int(dx/84),row=int(dy/temperatureStrideY);if(col>=5||row>=4||dx-col*84>=80||dy-row*temperatureStrideY>=temperatureTileHeight)return -1;return row*5+col;}
inline int nextVolume(int value){return value>=100?0:std::min(100,std::max(0,value)+20);}
inline Rect button(int group,int ordinal,int count){
    if(group==0){if(ordinal==0)return {x[group]+8,groupY[group]+33,width[group]-16,56};
        float w=(width[group]-22)/2;
        if(ordinal>=3){int i=ordinal-3;float aw=(width[group]-34)/4;return {x[group]+8+(i%4)*(aw+6),groupY[group]+194+(i/4)*27,aw,23};}
        int i=ordinal-1,rows=std::min(count,4)/2;float h=std::min(42.f,81.f/std::max(1,rows));
        return {x[group]+8+(i%2)*(w+6),groupY[group]+97+(i/2)*h,w,h-6};}
    if(group==6){
        if(ordinal<3){float w=(width[group]-28)/3;return {x[group]+8+ordinal*(w+6),groupY[group]+33,w,28};}
        int i=ordinal-3;float w=(width[group]-52)/7;
        return {x[group]+8+i*(w+6),groupY[group]+80,w,44};}
    int columns=group==1?(count>12?3:2):group==4?1:group==2?3:2;int rows=(count+columns-1)/columns;float h=std::min(48.f,(groupHeight[group]-33)/std::max(1,rows)),w=(width[group]-16-(columns-1)*6)/columns;return {x[group]+8+(ordinal%columns)*(w+6),groupY[group]+33+(ordinal/columns)*h,w,h-6};
}
struct PointerGrip {
    bool held=false,releaseTrigger=false;
    float handClosure=0;
    void update(float grip,float trigger,bool menu){held=held?grip>.45f:grip>.6f;if(menu||held)releaseTrigger=true;else if(trigger<=.3f)releaseTrigger=false;if(!menu&&!held&&!releaseTrigger)handClosure=trigger;}
};
}
