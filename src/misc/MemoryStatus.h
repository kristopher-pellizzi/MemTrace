#include "pin.H"

#ifndef MEMORYSTATUS
#define MEMORYSTATUS

uint8_t* cutUselessBits(uint8_t* uninitializedInterval, ADDRINT addr, UINT32 byteSize);
uint8_t* addOffset(uint8_t* data, unsigned offset, unsigned* srcShadowSize, unsigned srcByteSize);

#endif //MEMORYSTATUS