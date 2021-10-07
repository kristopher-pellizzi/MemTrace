#include <string>
#include "misc/CeilToMultipleOf8.h"

using std::string;

#ifndef SHDWREGISTER
#define SHDWREGISTER

// ShadowRegisters size is expressed in bits (e.g. size=64 means the register is a 64 bits register)
class ShadowRegister{
    protected:
        string name;
        uint8_t* content;
        unsigned size;
        unsigned byteSize;
        unsigned shadowSize;

        static uint8_t* initData;
        static unsigned biggestShadowSize;

        static uint8_t* getFullyInitializedData();

    public:
        ShadowRegister(const char* name, unsigned size, uint8_t* content) :
            name(string(name)),
            content(content),
            size(size),
            byteSize(size / 8),
            shadowSize(ceilToMultipleOf8(byteSize) / 8)
        {
            if(shadowSize > ShadowRegister::biggestShadowSize){
                biggestShadowSize = shadowSize;
            }
        }

        virtual ~ShadowRegister(){}

        string& getName();
        uint8_t* getContentPtr();
        unsigned getSize();
        unsigned getByteSize();
        unsigned getShadowSize();
        virtual void setAsInitialized(uint8_t* data);
        virtual void setAsInitialized();
        virtual uint8_t* getContentStatus();
        virtual bool isUninitialized();
        virtual bool isHighByte();
};


class ShadowOverwritingSubRegister : public ShadowRegister{
    private:
        uint8_t* superRegisterContent;
    
    public:
        ShadowOverwritingSubRegister(const char* name, unsigned size, uint8_t* content, uint8_t* superRegisterContent) :
            ShadowRegister(name, size, content),
            superRegisterContent(superRegisterContent)
        {}

        void setAsInitialized(uint8_t* data) override;
        uint8_t* getContentStatus() override;
};


class ShadowHighByteSubRegister : public ShadowRegister{
    public:
        ShadowHighByteSubRegister(const char* name, unsigned size, uint8_t* content) :
            ShadowRegister(name, size, content)
        {}

        void setAsInitialized(uint8_t* data) override;
        uint8_t* getContentStatus() override;
        bool isUninitialized() override;
        bool isHighByte() override;
};

#endif //SHDWREGISTER