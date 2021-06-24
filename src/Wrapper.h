#include <cstring>
#include <unistd.h>
#include <cctype>

using std::cerr;
using std::endl;

class ArgBytes{
    private:
        unsigned char* rawBytes;
        size_t size; // NOTE: last NULL pointer is not considered in |size|
        unsigned char** argArray;

        unsigned char** split();
        static unsigned char* strip(unsigned char* s);

        // NOTE: the following 2 methods accept an unsigned char** argument just to allow chaining
        // those function calls, similarly as it is done in a real functional programming language.
        // However, it must hold that the passed pointer is the same as the one stored in argArray,
        // which is an array thought to be changed according to the performed operations, 
        // that will simply change the stored pointers, without using free and malloc, and
        // avoiding memory copies, which are known to be quite expensive operations.
        // Also, note that, despite the functions signatures and behavior are similar to their usual 
        // functional equivalent, they are not meant to be really functional. 
        // Also, there are additional constraints for |map|, which can only use functions returning a very specific type.
        // These functions have simply been thought to make it easy to understand what public methods using them are doing.
        
        // Only KEEP in the array those elements that make predicate |f| TRUE
        template<class Func>
        unsigned char** filter(Func f, unsigned char** arr){
            if(arr != argArray){
                cerr << "The passed pointer is not valid. Program will terminate." << endl;
                exit(1);
            }

            size_t insertionIndex = 1;
            size_t newSize = size;
            // NOTE: arr[0] is reserved for executable name, it must not be transformed.
            // Start from arr[1] up to arr[size]. arr[size + 1] will contain NULL
            for(size_t i = 1; i <= size; ++i){
                if(!f(arr[i])){
                    --newSize;
                    continue;
                }
                arr[insertionIndex] = arr[i];
                ++insertionIndex;
            }
            arr[insertionIndex] = NULL;
            size = newSize;
            return arr;
        }

        // Apply function |f| to every element of |arr|, and return a new array, where each element is f(arr[i])
        // Of course, since C++ is statically typed, function |f| MUST return an unsigned char* type
        template<class Func>
        unsigned char** map(Func f, unsigned char** arr){
            if(arr != argArray){
                cerr << "The passed pointer is not valid. Program will terminate." << endl;
                exit(1);
            }
            // NOTE: arr[0] is reserved for executable name, it must not be transformed.
            // Start from arr[1] up to arr[size]. arr[size + 1] will contain NULL
            for(size_t i = 1; i <= size; ++i){
                arr[i] = f(arr[i]);
            }

            return arr;
        }


    public:
        ArgBytes(unsigned char* rawBytes) : rawBytes(rawBytes){};

        ~ArgBytes();

        unsigned char** getArgs();

        size_t getSize();
};