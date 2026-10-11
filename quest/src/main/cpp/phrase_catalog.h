#pragma once
#include <json.hpp>
#include <string>
#include <vector>

class PhraseCatalog {
    std::vector<std::string> phrases_{
        "Hello, I am Pepper.", "How are you?", "Could you repeat that, please?",
        "Thank you.", "All right.", "One moment, please."
    };
public:
    // Keep the last complete robot catalogue through missing snapshots and
    // disconnects. Built-in phrases also populate the very first offline start.
    const std::vector<std::string>& update(const nlohmann::json& state){
        auto found=state.find("phrases");
        if(found==state.end()||!found->is_array()||found->empty()||found->size()>24)return phrases_;
        std::vector<std::string> next;
        for(const auto& phrase:*found){
            if(!phrase.is_string())return phrases_;
            auto text=phrase.get<std::string>();
            if(text.empty()||text.size()>400)return phrases_;
            next.push_back(std::move(text));
        }
        phrases_=std::move(next);
        return phrases_;
    }
};
