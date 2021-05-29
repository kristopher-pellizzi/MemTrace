#include <string>

static void toUppercase(std::string& s){
    for(std::size_t i = 0; i < s.size(); ++i){
        // 97 = 'a', 122 = 'z'
        if(s[i] >= 97 && s[i] <= 122){
            s[i] = s[i] - 32;
        }
    }
}

namespace HeuristicStatus{
    enum Status{
        ON, 
        OFF,
        LIBS
    };    

    Status fromString(std::string& s){
        toUppercase(s);
        if(s.size() == 2 && s.compare("ON") == 0)
            return ON;
        if(s.size() == 3 && s.compare("OFF") == 0)
            return OFF;
    
        // If it is none of the previous, it is not a valid choice, enable the default choice,
        // that is enable the heuristic only on libraries.
        // Note that the same choice is done even if the user explicitly sent "LIBS" as a choice.
        return LIBS;
    }
}

