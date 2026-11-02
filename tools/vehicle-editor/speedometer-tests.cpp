#include "Speedometer.h"
#include "VehiclePlacement.h"
#include "Preferences.h"
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace VehiclePlacement;
static void check(bool condition,const char *message) {
    if(!condition) throw std::runtime_error(message);
}
static bool close(float a,float b) { return std::abs(a-b)<.00001f; }
static void same(Vec a,Vec b) { check(dot(a-b,a-b)<1e-9f,"placement transform mismatch"); }
int main() {
    try {
        check(close(Speedometer::kmh(1),180),"engine speed conversion");
        check(close(Speedometer::displaySpeed(77.248512f,false),48),"mph conversion");
        check(close(Speedometer::displaySpeed(77.248512f,true),77.248512f),"km/h conversion");
        check(Speedometer::kmh(-1)==0 && Speedometer::kmh(std::numeric_limits<float>::quiet_NaN())==0,"bad speed sanitization");
        check(Speedometer::dialMaximum(180,false)==140 && Speedometer::dialMaximum(180,true)==220,"rounded headroom");
        check(Speedometer::dialMaximum(300,false)>Speedometer::dialMaximum(120,false),"per-vehicle scale");
        check(Speedometer::dialMaximum(0,true)==220,"missing handling fallback");
        check(Speedometer::needleAngle(400,140)==Speedometer::needleAngle(140,140),"needle clamps at end");
        check(Speedometer::displaySpeed(400,true)==400,"numeric speed remains uncapped");
        const char *suffix[]={"X","Y","Z","Pitch","Yaw","Roll","Size"};
        Store store;
        for(int i=0;i<7;++i) store.personal.entries[section(2,101)][std::string("Speedometer")+suffix[i]]=i==6?125.f:i*7.f;
        std::string error;
        Pack roundtrip; std::istringstream input(store.personal.text());
        check(roundtrip.parse(input,error),"new keys roundtrip");
        check(roundtrip.entries==store.personal.entries,"precision roundtrip");
        auto fallback=legacyDefaults(2,101,false);
        auto resolved=resolve(store,2,101,fallback);
        check(resolved["SpeedometerSize"]==125 && resolved["SpeedometerYaw"]==28,"saved values resolved");
        check(resolve(store,0,101,fallback)["SpeedometerSize"]==100,"modelset isolation");
        check(resolve(store,2,102,fallback)["SpeedometerSize"]==100,"vehicle isolation");
        store.templates.entries[section(2,101)]["SpeedometerX"]=9;
        check(resolve(store,2,101,fallback)["SpeedometerX"]==0,"personal priority");
        store.personal.entries[section(2,101)].erase("SpeedometerX");
        check(resolve(store,2,101,fallback)["SpeedometerX"]==9,"reset reveals template");
        Pack invalid; std::istringstream bad("[Placement]\nVersion=1\n[classic:101]\nSpeedometerSize=0\n");
        check(!invalid.parse(bad,error),"reject invalid scale");
        // The pivot is only translated by the dashboard and offsets; rotation
        // and style/size changes cannot move the bottom-centre origin.
        Basis parent; parent.position=Vec(1,2,3); parent.turn(13,27,-15);
        auto values=legacyDefaults(0,101,false); values["DashSize"]=150;
        auto first=namedPanel(parent,values,"Speedometer");
        same(first.position,parent.point(Vec(0,.12f,.33f)));
        values["SpeedometerPitch"]=73; values["SpeedometerRoll"]=-34; values["SpeedometerSize"]=200;
        same(first.position,namedPanel(parent,values,"Speedometer").position);
        for(bool analog:{false,true}) for(bool metric:{false,true}) {
            Speedometer::Mesh mesh;
            Speedometer::build(mesh,analog,metric,400,Speedometer::dialMaximum(180,metric));
            check(!mesh.vertices.empty() && mesh.vertices.size()%3==0 && mesh.vertices.size()<65000,"bounded triangle mesh");
            float low=1, left=1, right=-1;
            for(auto v:mesh.vertices) {
                check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"finite vertices");
                low=std::min(low,v.y); left=std::min(left,v.x); right=std::max(right,v.x);
            }
            check(close(low,0) && close(left,-right),"both meshes have a bottom-centre pivot");
        }
        editor::Preferences prefs; prefs.set("Element",7);
        std::istringstream prefsInput(prefs.text()); editor::Preferences loaded;
        check(loaded.parse(prefsInput,error) && loaded.get("Element")==7,"editor restores speedometer selection");
        std::cout<<"Speedometer conversion, scale, geometry, placement layers and pivot checks passed.\n";
    } catch(const std::exception &e) { std::cerr<<e.what()<<"\n"; return 1; }
}
