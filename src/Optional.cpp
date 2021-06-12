#include "Optional.h"

template <class T>
T& Optional<T>::value() const{
    return val;
}

template <class T>
bool Optional<T>::hasValue() const{
    return !isNone;
}