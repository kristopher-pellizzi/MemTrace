#include <map>
#include <queue>
#include "AccessIndex.h"
#include "MemoryAccess.h"

#ifndef TAGMANAGER
#define TAGMANAGER

using std::map;
using std::pair;
using std::queue;

#define Access pair<AccessIndex, MemoryAccess>
typedef unsigned long long tag_t;

class TagManager{
    private:
        map<tag_t, Access> tagToAccess;
        map<Access, tag_t> accessToTag;
        map<tag_t, unsigned> referenceCount;
        queue<tag_t> freeTags;
        tag_t nextUnusedTag;
        const Access errorAccess;

        TagManager();
        tag_t newTag();
        void freeTag(tag_t tag);

    public:
        static TagManager& getInstance();
        const Access& getAccess(tag_t tag);
        const tag_t getTag(Access access);  
        void increaseRefCount(tag_t tag);
        void decreaseRefCount(tag_t tag);
        void decreaseRefCount(set<tag_t>& tags);
};

#undef Access
#endif //TAGMANAGER