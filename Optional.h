// Keep this special class visible only here, as it is used to create an empty Optional object
namespace{
    class EmptyOptional{
        public:
            void doNothing(){}
        
    };
}

// This is used to easily allow the user to specify an empty Optional object
EmptyOptional None;

template <class T>
class Optional{
    private:
        T val;
        bool isNone;

        // NOTE: as stated by the method's name itself, this is a useless method doing absolutely nothing.
        // This is implemented as a private method, so that anything outside this class member functions
        // cannot get access to that.
        // This trick is used in order to avoid the compiler raise an unused variable error
        // on None. In future, a better solution should be found. This should be only temporary
        void useless(){
            None.doNothing();
        }

    public:
        Optional(){
            isNone = true;
        }

        Optional(EmptyOptional none){
            isNone = true;
        }

        Optional(T value) : val(value){
            isNone = false;
        }

        operator bool() const{
            return !isNone;
        }

        T& value() const;

        bool hasValue() const;
};