#include<iostream>


struct Delimiter {
    char delimiter;
    mutable bool first = true;
    friend std::ostream& operator<<(std::ostream& out, const Delimiter& v){
        if(!v.first)
            out << v.delimiter;
        v.first = false;
        return out;
    }
};

