#include "TagManager.h"

#define Access pair<AccessIndex, MemoryAccess>

TagManager::TagManager() : nextUnusedTag(0), errorAccess(Access(AccessIndex(0,0), MemoryAccess())){}

TagManager& TagManager::getInstance(){
    static TagManager instance;

    return instance;
}

tag_t TagManager::newTag(){
    tag_t ret = 0;

    if(freeTags.size() != 0){
        ret = freeTags.front();
        freeTags.pop();
    }
    else if(nextUnusedTag == (tag_t) -1){
        std::cerr << "Error: no more tags available" << std::endl;
        exit(EXIT_FAILURE);
    }
    else{
        ret = nextUnusedTag++;
    }

    referenceCount[ret] = 0;
    return ret;
}

const Access& TagManager::getAccess(tag_t tag){
    auto elem = tagToAccess.find(tag);

    if(elem != tagToAccess.end())
        return elem->second;

    /*
        This should never happen. If the tags returned by the tag manager are used correctly,
        there will always be an access associated to a given tag
    */
    #ifdef DEBUG
    std::cerr << std::endl << "No access found associated to tag " << std::dec << tag << std::endl;
    #endif
    return errorAccess;
}

const tag_t TagManager::getTag(Access access){
    auto elem = accessToTag.find(access);

    if(elem != accessToTag.end())
        return elem->second;
    
    tag_t ret = newTag();
    tagToAccess[ret] = access;
    accessToTag[access] = ret;

    return ret;
}

void TagManager::increaseRefCount(tag_t tag){
    if(referenceCount.find(tag) == referenceCount.end())
        return;

    ++referenceCount[tag];
}

void TagManager::decreaseRefCount(tag_t tag){
    if(referenceCount.find(tag) == referenceCount.end())
        return;

    /*
        If there's only 1 reference left, remove the tag from all the structures and add it to the free list
    */
    if(referenceCount[tag] <= 1){
        #ifdef DEBUG
        std::cerr << "[TagManager] Freeing tag " << std::dec << tag << std::endl;
        #endif
        referenceCount.erase(tag);
        auto accessIter = tagToAccess.find(tag);
        // Should always be different from tagAccess.end()
        if(accessIter != tagToAccess.end()){
            Access& access = accessIter->second;
            accessToTag.erase(access);
        }
        tagToAccess.erase(tag);

        freeTags.push(tag);
    }
    // Otherwise simply reduce the reference count by 1
    else{
        --referenceCount[tag];
    }
}

void TagManager::decreaseRefCount(set<tag_t>& tags){
    for(auto iter = tags.begin(); iter != tags.end(); ++iter){
        decreaseRefCount(*iter);
    }
}