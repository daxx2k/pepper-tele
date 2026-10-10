#include "../quest/src/main/cpp/exit_flow.h"
#include <cassert>
#include <cstdio>

int main() {
    using D = exit_flow::Decision;
    nlohmann::json stopped = {{"motion_diagnostics", {{"stopping", false}, {"stop_error", ""}}}};
    auto decision = [&](const nlohmann::json& status, bool connected=true, bool previously=true,
            bool armed=false, unsigned ack=2, double age=10, double now=100) {
        return exit_flow::evaluate(connected, previously, armed, ack, 1, now, 5000, age, status);
    };
    assert(decision(stopped)==D::RestoreNormal); // actual nested bridge status
    assert(decision(stopped,false,false)==D::CloseLocal); // never controlled a robot
    assert(decision(stopped,false,true)==D::OfferLocalClose); // lost connection: no normal-mode claim
    assert(decision(stopped,true,true,false,2,10,5001)==D::OfferLocalClose);
    assert(decision(stopped,true,true,true)==D::Wait);
    assert(decision(stopped,true,true,false,1)==D::Wait);
    assert(decision(stopped,true,true,false,2,750)==D::Wait);
    assert(decision(stopped,true,true,false,2,-1)==D::Wait);
    assert(decision(nlohmann::json::object())==D::Wait);
    assert(decision({{"stopping",false},{"stop_error",""}})==D::Wait); // incorrect top-level shortcut
    auto pending=stopped;pending["motion_diagnostics"]["stopping"]=true;assert(decision(pending)==D::Wait);
    auto error=stopped;error["motion_diagnostics"]["stop_error"]="STOP failed";assert(decision(error)==D::Wait);
    auto malformed=stopped;malformed["motion_diagnostics"]["stopping"]="false";assert(decision(malformed)==D::Wait);
    auto missing=stopped;missing["motion_diagnostics"].erase("stop_error");assert(decision(missing)==D::Wait);
    std::puts("PASS 14 exit cases: nested STOP acknowledgement, offline close, lost connection, stale/invalid status and stop failure safeguards");
}
