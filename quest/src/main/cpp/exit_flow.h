#pragma once
#include <json.hpp>

namespace exit_flow {
enum class Decision { Wait, CloseLocal, RestoreNormal, OfferLocalClose };

inline Decision evaluate(bool connected, bool previouslyConnected, bool armed,
        unsigned acknowledged, unsigned before, double now, double deadline,
        double snapshotAge, const nlohmann::json& snapshot) {
    if (!connected) return previouslyConnected ? Decision::OfferLocalClose : Decision::CloseLocal;
    if (now > deadline) return Decision::OfferLocalClose;
    if (armed || acknowledged <= before || snapshotAge < 0 || snapshotAge >= 750)
        return Decision::Wait;
    auto diagnostics = snapshot.find("motion_diagnostics");
    if (diagnostics == snapshot.end() || !diagnostics->is_object()) return Decision::Wait;
    auto stopping = diagnostics->find("stopping"), error = diagnostics->find("stop_error");
    if (stopping == diagnostics->end() || !stopping->is_boolean() || stopping->get<bool>()
            || error == diagnostics->end() || !error->is_string() || !error->get<std::string>().empty())
        return Decision::Wait;
    return Decision::RestoreNormal;
}
}
