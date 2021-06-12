#include "pin.H"
#include "AccessIndex.h"
#include <set>
#include <iterator>

using std::iterator;
using std::set;

class InitializedMemory{
    private:
        set<AccessIndex> initializations;
        InitializedMemory* prev;
        ADDRINT frameBeginning;

    public:
        InitializedMemory(InitializedMemory* prev, ADDRINT frameBeginning):
            prev(prev),
            frameBeginning(frameBeginning)
        {}

        std::pair<set<AccessIndex>::iterator, bool> insert(const AccessIndex& toInsert);

        void insert(set<AccessIndex>::iterator firstIterator, set<AccessIndex>::iterator lastIterator);

        set<AccessIndex>::const_iterator find(const AccessIndex& val) const;

        set<AccessIndex>::const_iterator begin() const;

        set<AccessIndex>::const_iterator end() const;

        InitializedMemory* deleteFrame();

        std::pair<int, int> getUninitializedInterval(AccessIndex& ai);
};