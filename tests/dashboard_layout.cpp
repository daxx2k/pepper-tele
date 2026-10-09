#include "../quest/src/main/cpp/dashboard_layout.h"
#include "../quest/src/main/cpp/thermal_color.h"
#include "../quest/src/main/cpp/camera_fusion.h"
#include "../quest/src/main/cpp/controller_presence.h"
#include "../quest/src/main/cpp/dashboard_theme.h"
#include <cassert>
#include <cstdio>
int main(){using namespace dashboard_layout;
    for(auto r:{headerLayout,headerHelp})assert(r.w>=48&&r.h>=48&&r.x>=20&&r.x+r.w<=1380&&r.y>=0&&r.y+r.h<=56);
    assert(headerLayout.x+headerLayout.w<headerHelp.x);
    assert(x[4]<fusion.x&&x[0]>fusion.x+fusion.w);
    for(int group=0;group<7;group++){assert(x[group]>=0&&x[group]+width[group]<=1400);
        if(group==3||group==5){assert(width[group]==0);continue;}
        for(int count=1;count<=(group==1?32:group==6?10:group==0?15:8);count++)for(int i=0;i<count;i++){auto r=button(group,i,count);assert(r.x>=x[group]&&r.x+r.w<=x[group]+width[group]+.01f);assert(r.y>=groupY[group]+33&&r.y+r.h<=groupY[group]+groupHeight[group]+.01f&&r.h>0);}
        for(int other=group+1;other<7;other++){if(other==3||other==5)continue;assert(x[group]+width[group]<=x[other]||x[other]+width[other]<=x[group]||groupY[group]+groupHeight[group]<=groupY[other]||groupY[other]+groupHeight[other]<=groupY[group]);}
    }
    assert(clearLaser.x>=lidarCard.x&&clearLaser.x+clearLaser.w<=lidarCard.x+lidarCard.w&&clearLaser.y>=lidarCard.y&&clearLaser.y+clearLaser.h<=lidarCard.y+lidarCard.h);
    assert(button(6,0,10).h>=26&&button(6,1,10).x>button(6,0,10).x);
    assert(displayTemperature(0,true)==32&&displayTemperature(100,true)==212&&displayTemperature(37,false)==37);
    assert(temperatureUnit.x+temperatureUnit.w<=temperatures.x+temperatures.w);
    assert(depthCard.y+depthCard.h==950&&lidarCard.y+lidarCard.h==950&&tablet.y+tablet.h==950);
    assert(voiceButton(false,5,6).y+voiceButton(false,5,6).h<voiceButton(true,0,6).y);
    assert(clearLaser.w==44&&clearLaser.x+clearLaser.w==lidarCard.x+lidarCard.w-8&&clearLaser.y+clearLaser.h==lidarCard.y+lidarCard.h-8);
    assert(width[0]>200&&button(0,0,4).h>button(0,1,4).h&&button(0,0,4).w>button(0,1,4).w);
    assert(button(0,1,8).y>button(0,0,8).y+button(0,0,8).h);
    assert(button(0,3,11).y>button(0,2,11).y+button(0,2,11).h);
    assert(button(0,3,11).y==button(0,4,11).y&&button(0,7,11).y>button(0,6,11).y);
    assert(std::abs(tabletScreen.w/tabletScreen.h-1.6f)<.001f);
    assert(tabletScreen.x>=tablet.x&&tabletScreen.x+tabletScreen.w<=tablet.x+tablet.w&&tabletScreen.y>=tablet.y&&tabletScreen.y+tabletScreen.h<=tablet.y+tablet.h);
    assert(button(6,0,10).y==button(6,1,10).y&&button(6,1,10).y==button(6,2,10).y);
    assert(button(6,2,10).x>button(6,1,10).x&&button(6,3,10).y>button(6,0,10).y+button(6,0,10).h);
    for(auto r:{windowMove,windowPin})assert(r.y>996&&r.y+r.h<=1054&&r.x>=20&&r.x+r.w<=1380);
    assert(windowPin.x>windowMove.x+windowMove.w);
    assert(windowHover(windowMove.x,windowMove.y)&&windowHover(windowPin.x+20,windowPin.y+20));
    assert(!windowHover(500,1000)&&!windowHover(700,900));
    assert(grabScale(.8f,0)==.8f&&grabScale(.8f,.1f)<.8f&&grabScale(.8f,-.1f)>.8f);
    assert(grabScale(.8f,10)==.65f&&grabScale(.8f,-10)==1.f);
    auto cold=thermalColor(20,false),warm=thermalColor(60,false),alarm=thermalColor(20,true),missing=thermalColor(NAN,false);
    assert(cold[2]>cold[0]&&warm[0]>warm[2]);
    assert(alarm[0]==1&&alarm[1]<.2f&&alarm[2]<.2f);
    assert(thermalColor(NAN,true)==alarm&&std::isfinite(missing[0]));
    auto lowBattery=dashboard_theme::batteryColor(.1f),midBattery=dashboard_theme::batteryColor(.5f),highBattery=dashboard_theme::batteryColor(.9f);
    assert(lowBattery[0]>lowBattery[1]&&highBattery[1]>highBattery[0]&&midBattery[0]>midBattery[2]);
    assert(dashboard_theme::batteryColor(-1)==lowBattery&&dashboard_theme::batteryColor(2)==highBattery);
    assert(dashboard_theme::batteryColor(NAN)==dashboard_theme::secondary);
    for(float temp:{0.f,30.f,50.f,100.f}){auto c=thermalColor(temp,false);assert(c[1]>.2f);for(float channel:c)assert(channel>=0&&channel<=1);}
    assert(cameras[0].x==cameras[1].x&&cameras[1].y==cameras[0].y+cameras[0].h+cameraGap);
    assert(cameraGap==20&&cameraBlendPixels<.03f*cameras[1].h&&cameraCornerRadius<=8);
    for(int c=0;c<2;++c){auto fullCamera=contain(cameras[c],320,240);assert(fullCamera.w==cameras[c].w&&fullCamera.h==cameras[c].h);assert(cameras[c].x>=fusion.x&&cameras[c].x+cameras[c].w<=fusion.x+fusion.w&&cameras[c].y+cameras[c].h<=fusion.y+fusion.h);}
    auto full=contain(cameras[2],320,240),wide=contain(cameras[2],1920,1080),square=contain(cameras[2],512,512);
    assert(full.w<=cameras[2].w&&full.h<=cameras[2].h&&std::abs(full.w/full.h-4.f/3)<1e-5);
    assert(wide.w<=cameras[2].w&&wide.h<=cameras[2].h&&std::abs(wide.w/wide.h-16.f/9)<1e-5);
    assert(square.w==square.h&&square.w==cameras[2].h&&square.x>cameras[2].x);
    assert(cameras[2].y>fusion.y+fusion.h&&cameras[2].w<277&&cameras[2].h<208);
    assert(lidar.y>fusion.y+fusion.h&&lidar.w<424&&lidar.h<208);
    assert(cameras[2].x+cameras[2].w<lidar.x&&lidar.x+lidar.w<=lidarCard.x+lidarCard.w);
    assert(tablet.y>fusion.y+fusion.h&&tablet.y+tablet.h<=950);
    assert(tablet.y<=cameras[2].y&&tablet.y+tablet.h>=cameras[2].y+cameras[2].h);
    assert(groupY[4]>temperatures.y+temperatures.h&&x[4]==comparison.x);
    assert(fusion.w==460&&cameras[0].w==444&&depthCard.x+depthCard.w<=lidarCard.x&&lidarCard.x+lidarCard.w<=fusion.x+fusion.w);
    assert(temperatures.w==440&&temperatures.h>240&&comparison.y+comparison.h<temperatures.y);
    assert(groupHeight[4]<170);
    for(int i=0;i<20;++i){auto tile=temperatureTile(i);assert(temperatureAt(tile.x+2,tile.y+2)==i);assert(tile.h>40&&tile.y+tile.h<temperatures.y+temperatures.h-18);}
    assert(temperatureAt(0,0)==-1&&temperatureAt(temperatures.x+92,temperatures.y+33)==-1);
    int volume=0;for(int expected:{20,40,60,80,100,0}){volume=nextVolume(volume);assert(volume==expected);}
    PointerGrip g;g.update(0,.8f,false);assert(g.handClosure==.8f);
    g.update(.8f,.9f,false);assert(g.held&&g.handClosure==.8f);
    g.update(.5f,.9f,false);assert(g.held);
    g.update(.4f,.9f,false);assert(!g.held&&g.releaseTrigger&&g.handClosure==.8f);
    g.update(0,0,false);assert(!g.releaseTrigger&&g.handClosure==0);
    g.update(0,.7f,false);assert(g.handClosure==.7f);
    g.update(0,.9f,true);assert(g.handClosure==.7f);
    g.update(0,.9f,false);assert(g.releaseTrigger&&g.handClosure==.7f);
    g.update(0,0,false);assert(g.handClosure==0);
    auto top=camera_fusion::ray(0,.5f,.5f),bottom=camera_fusion::ray(1,.5f,.5f);
    assert(std::abs(top.yaw)<1e-5&&std::abs(top.elevation)<1e-5&&std::abs(bottom.elevation+40)<1e-4);
    assert(camera_fusion::ray(0,.5f,1).elevation<camera_fusion::ray(1,.5f,0).elevation);
    for(int camera=0;camera<2;++camera){auto mesh=camera_fusion::mesh(camera,fusionImage);assert(mesh.size()==32*24*24);
        float minU=1,maxU=0,minV=1,maxV=0;
        for(size_t i=0;i<mesh.size();i+=4){float px=(mesh[i]+1)*800,py=(1-mesh[i+1])*530;assert(std::isfinite(px)&&std::isfinite(py));assert(px>=fusionImage.x-.01f&&px<=fusionImage.x+fusionImage.w+.01f&&py>=fusionImage.y-.01f&&py<=fusionImage.y+fusionImage.h+.01f);
            minU=std::min(minU,mesh[i+2]);maxU=std::max(maxU,mesh[i+2]);minV=std::min(minV,mesh[i+3]);maxV=std::max(maxV,mesh[i+3]);}
        assert(minU==0&&maxU==1&&minV==0&&maxV==1);}
    ControllerPresence presence;assert(!presence.update(0,true,true)&&presence.bothHeld);
    assert(!presence.update(1,false,true)&&presence.bothHeld);assert(!presence.update(1.1,true,true));
    assert(!presence.update(2,true,false)&&presence.bothHeld);assert(!presence.update(2.5,true,false)&&presence.bothHeld);
    assert(presence.update(2.9,true,false)&&!presence.bothHeld);assert(!presence.update(3,false,false));
    assert(!presence.update(4,true,true));assert(presence.update(4.01,true,true,false)&&!presence.bothHeld);
    assert(!presence.update(4.1,false,false,false));assert(!presence.update(5,true,true));
    assert(!presence.update(6,false,true)&&presence.bothHeld);assert(presence.update(6.9,false,true)&&!presence.bothHeld);
    ControllerPresence startup;assert(!startup.update(0,false,false)&&!startup.bothHeld);
    std::puts("PASS narrow dashboard, full unwarped cameras, sensor cards, hover handle, pin, grab scaling, thermal colours and input isolation");
}

