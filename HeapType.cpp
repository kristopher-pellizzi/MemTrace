#include "HeapType.h"

HeapType::HeapType(HeapEnum type){
    if(type == HeapEnum::MMAP){
        this->type = HeapEnum::INVALID;
    }
    else{
        this->type = type;
    }

    this->shadowMemoryIndex = 0;
}

HeapType::HeapType(HeapEnum type, ADDRINT ptr){
    this->type = type;
    if(type != HeapEnum::MMAP){
        this->shadowMemoryIndex = 0;
    }
    else{
        this->shadowMemoryIndex = ptr;
    }
}

ADDRINT HeapType::getShadowMemoryIndex(){
    return type == HeapEnum::MMAP ? shadowMemoryIndex.value() : 0;
}

bool HeapType::isValid() const{
    return type != HeapEnum::INVALID;
}

bool HeapType::isNormal(){
    return type == HeapEnum::NORMAL;
}

HeapType::operator bool() const{
    return isValid();
}
