#ifndef HEAPTYPE
#define HEAPTYPE

#include "Optional.h"
#include "HeapEnum.h"
#include "pin.H"

class HeapType {
    private:
        HeapEnum type;
        Optional<ADDRINT> shadowMemoryIndex;

    public:
        HeapType(HeapEnum type);

        HeapType(HeapEnum type, ADDRINT ptr);

        ADDRINT getShadowMemoryIndex();

        bool isValid() const;

        bool isNormal();

        // By overriding operator bool(), we allow the user to use a HeapType object as a boolean value,
        // simplifying the usage as a boolean expression. Note that the same can be achieved by using method
        // |isValid()|
        operator bool() const;
};


#endif // HEAPTYPE