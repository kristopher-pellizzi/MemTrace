#include <cstring>
#include "ShadowRegister.h"

unsigned ShadowRegister::biggestShadowSize = 0;
uint8_t* ShadowRegister::data = NULL;

uint8_t* ShadowRegister::getFullyInitializedData(){
    if(data == NULL){
        data = (uint8_t*) malloc(sizeof(uint8_t) * biggestShadowSize);
        memset(data, 0xff, biggestShadowSize);
    }

    return data;
}

string& ShadowRegister::getName(){
    return name;
}

unsigned ShadowRegister::getSize(){
    return size;
}

unsigned ShadowRegister::getByteSize(){
    return byteSize;
}

unsigned ShadowRegister::getShadowSize(){
    return shadowSize;
}

uint8_t* ShadowRegister::getContentPtr(){
    return content;
}

// Sets register's content as initialized copying the pattern set into |data|
void ShadowRegister::setAsInitialized(uint8_t* data){
    // If the shadowSize is equal to the exact size of the register, then simply copy the data into register
    if(byteSize / 8 == shadowSize){
        memcpy(content, data, shadowSize);
        return;
    }
    
    // Set every not interesting bit to 1, so that it will be never detected as uninitialized
    uint8_t mask = 0xff << (byteSize % 8);
    // Set to 0 register's bits
    *content &= mask;
    mask = ~mask;
    uint8_t maskedData = mask & *data;
    // Set first 8 most significant register's bits to the corresponding bits in |data|
    *content |= maskedData;

    // Copy the remaining bits
    memcpy(content + 1, data + 1, shadowSize - 1);
}

void ShadowRegister::setAsInitialized(){
    setAsInitialized(getFullyInitializedData());
}

// Requires the user to manually free the returned pointer
uint8_t* ShadowRegister::getContentStatus(){
    uint8_t* ret = (uint8_t*) malloc(sizeof(uint8_t) * shadowSize);
    unsigned offset = byteSize % 8;
    if(offset == 0){
        memcpy(ret, content, shadowSize);
    }
    else{
        uint8_t mask = 0xff << offset;
        *ret = *content | mask;

        memcpy(ret + 1, content + 1, shadowSize - 1);
    }

    return ret;
}


bool ShadowRegister::isUninitialized(){
    uint8_t* currShadowByte = content;
    uint8_t shadowByte = *currShadowByte;
    unsigned offset = byteSize % 8;
    unsigned i = 0;

    if(offset != 0){
        uint8_t mask = 0xff << offset;
        if((shadowByte | mask) != 0xff){
            return true;
        }

        ++currShadowByte;
        ++i;
    }

    for(; i < shadowSize; ++i){
        shadowByte = *currShadowByte++;
        if(shadowByte != 0xff)
            return true;
    }

    return false;
}


bool ShadowRegister::isHighByte(){
    return false;
}



// Begin of ShadowOverwritingSubRegister section
void ShadowOverwritingSubRegister::setAsInitialized(uint8_t* data){
    // Copy register's bits by calling parent class' method
    ShadowRegister::setAsInitialized(data);

    // Now we must overwrite super-register's bits, setting them all to an INITIALIZED state
    uint8_t mask = 0xff;
    uint8_t* dst = superRegisterContent;
    unsigned ptrDiff = (unsigned) (content - superRegisterContent);
    for(unsigned i = 0; i < ptrDiff; ++i){
        *dst++ = mask;
    }

    if(byteSize / 8 != shadowSize){
        mask = 0xff << (byteSize % 8);
        *dst |= mask;
    }

}

uint8_t* ShadowOverwritingSubRegister::getContentStatus(){
    return ShadowRegister::getContentStatus();
}



// Begin of ShadowHighByteSubRegister section
void ShadowHighByteSubRegister::setAsInitialized(uint8_t* data){
    uint8_t mask = (uint8_t)(0xff << 2) + 1;
    *content &= mask;
    mask = ~mask;
    uint8_t maskedData = *data & mask;
    *content |= maskedData;
}

uint8_t* ShadowHighByteSubRegister::getContentStatus(){
    uint8_t mask = (uint8_t)(0xff << 2) + 1;
    uint8_t* ret = (uint8_t*) malloc(sizeof(uint8_t));
    *ret = *content | mask;

    return ret;
}

bool ShadowHighByteSubRegister::isUninitialized(){
    uint8_t mask = (uint8_t) (0xff << 2) + 1;
    uint8_t maskedContent = *content | mask;
    return maskedContent != 0xff;
}

bool ShadowHighByteSubRegister::isHighByte(){
    return true;
}