#include "PendingReads.h"
#include <iterator>
#include <iostream>

using std::cerr;
using std::endl;


bool IncreasingStartRangeSorter::operator()(const range_t& x, const range_t& y) const{
    if(x.first != y.first)
        return x.first < y.first;
    return x.second < y.second;
}

bool IncreasingEndRangeSorter::operator()(const range_t& x, const range_t& y) const{
    if(x.second != y.second)
        return x.second < y.second;
    return x.first < y.first;
}

map<unsigned, set<tag_t>> pendingUninitializedReads;
map<range_t, set<tag_t>, IncreasingStartRangeSorter> storedPendingUninitializedReads;

/*
    Whenever an uninitialized read writes a register, all of its sub-registers are overwritten.
    So, add the entry to the destination registers and to all their sub-registers.
    E.g. if an instruction writes eax, it will also completely overwrite ax, al and ah, thus overwriting the 
    information about uninitialized bytes in there. rax, instead, remains untouched, as it may still have an 
    uninitialized byte in a position not included in eax.
    It is responsibility of funtion |checkDestRegisters| to perform the required operations to check if
    super-registers should be kept in the structure or should be removed.
*/
void addPendingRead(list<REG>* regs, const MemoryAccess& ma){
    if(regs == NULL || !ma.getIsUninitializedRead())
        return;
    
    TagManager& tagManager = TagManager::getInstance();
    AccessIndex ai(ma.getAddress(), ma.getSize());
    std::pair<AccessIndex, MemoryAccess> entry(ai, ma);
    tag_t entry_tag = tagManager.getTag(entry);
    set<unsigned> toAdd;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();

    for(auto iter = regs->begin(); iter != regs->end(); ++iter){
        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        toAdd.insert(shadowReg);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);
        unsigned shadowByteSize = registerFile.getByteSize(*iter);

        for(auto aliasReg = aliasingRegisters.begin(); aliasReg != aliasingRegisters.end(); ++aliasReg){
            SHDW_REG shdw_reg = (SHDW_REG) *aliasReg;
            // Add the same entry for each sub-register
            if(registerFile.getByteSize(shdw_reg) < shadowByteSize){
                toAdd.insert(*aliasReg);
            }
        }

    }

    for(auto iter = toAdd.begin(); iter != toAdd.end(); ++iter){
        auto pendingReadIter = pendingUninitializedReads.find(*iter);
        // If there's already an accessSet associated with the register, simply add the entry
        // to that set
        if(pendingReadIter != pendingUninitializedReads.end()){
            pendingUninitializedReads[*iter].insert(entry_tag);
        }
        else{
            set<tag_t> s;
            s.insert(entry_tag);
            pendingUninitializedReads[*iter] = s;
        }

        tagManager.increaseRefCount(entry_tag);
    }
}

// Being an anonymous namespace, the following class is available only in this source file
namespace{
    class PendingReadsSorter{
        public:
            #define Access pair<AccessIndex, MemoryAccess>
            bool operator()(const Access& x, const Access& y){
                return x.second < y.second;
            }
            #undef Access
    };
}


static void addPendingRead(set<unsigned>& shdw_regs, set<tag_t>& accessSet){
    set<unsigned> toAdd;
    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    TagManager& tagManager = TagManager::getInstance();

    for(auto iter = shdw_regs.begin(); iter != shdw_regs.end(); ++iter){
        toAdd.insert(*iter);
 
        SHDW_REG shdw_iter = (SHDW_REG) *iter;
        unsigned byteSize = registerFile.getByteSize(shdw_iter);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(shdw_iter);
        
        for(auto aliasIter = aliasingRegisters.begin(); aliasIter != aliasingRegisters.end(); ++aliasIter){
            SHDW_REG shdw_alias = (SHDW_REG) *aliasIter;
            if(registerFile.getByteSize(shdw_alias) < byteSize){
                toAdd.insert(*aliasIter);
            }
        }
    }

    for(auto iter = toAdd.begin(); iter != toAdd.end(); ++iter){
        /*
            If dst register still has some pending read associated, we need to keep them
            in the access set to propagate
        */
        auto findIter = pendingUninitializedReads.find(*iter);
        if(findIter != pendingUninitializedReads.end()){
            pendingUninitializedReads[*iter].insert(accessSet.begin(), accessSet.end());
        }
        else{
            pendingUninitializedReads[*iter] = accessSet;
        }

        tagManager.increaseRefCount(accessSet);
    }
}


void propagatePendingReads(list<REG>* srcRegs, list<REG>* dstRegs){
    // If srcRegs or dstRegs are empty, there's nothing to propagate
    if(pendingUninitializedReads.size() == 0 || srcRegs == NULL || dstRegs == NULL)
        return;

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    set<unsigned, ShadowRegisterFile::DecresingSizeRegisterSorter> toPropagate;

    // Select all registers which should be propagated
    for(auto iter = srcRegs->begin(); iter != srcRegs->end(); iter++){
        // If source register is not uninitialized, it can't have accesses in |pendingUninitializedReads|
        if(!registerFile.isUninitialized(*iter) || registerFile.isUnknownRegister(*iter))
            continue;

        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        unsigned byteSize = registerFile.getByteSize(*iter);
        auto pendingReadIter = pendingUninitializedReads.find(shadowReg);
        set<tag_t> pendingReadSet = set<tag_t>();
        if(pendingReadIter != pendingUninitializedReads.end()){
            pendingReadSet = pendingReadIter->second;
            set<unsigned> correspondingRegisters = registerFile.getCorrespondingRegisters((SHDW_REG) shadowReg, dstRegs);
            
            addPendingRead(correspondingRegisters, pendingReadSet);
        }

        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);
        for(auto aliasIter = aliasingRegisters.begin(); aliasIter != aliasingRegisters.end(); ++aliasIter){
            SHDW_REG aliasShdwReg = (SHDW_REG) *aliasIter;

            // If the alias register is a sub-register
            if(registerFile.getByteSize(aliasShdwReg) < byteSize){
                pendingReadIter = pendingUninitializedReads.find(*aliasIter);
                // If it has an associated pending uninitialized read (it should have, this is simply to avoid run-time errors)
                if(pendingReadIter != pendingUninitializedReads.end()){
                    set<tag_t>& aliasPendingReadSet = pendingReadIter->second;
                    // Add to the set of registers still to be propagated only those sub-registers whose associated access set
                    // is different from the access set of the src register
                    if(aliasPendingReadSet != pendingReadSet){
                        toPropagate.insert(*aliasIter);
                    }
                }
            }
        }
    }

    // We must propagate changes done to sub-registers of src, if any
    for(auto iter = toPropagate.begin(); iter != toPropagate.end(); ++iter){
        auto pendingReadIter = pendingUninitializedReads.find(*iter);
        if(pendingReadIter != pendingUninitializedReads.end()){
            SHDW_REG shdw_iter = (SHDW_REG) *iter;
            set<tag_t>& pendingReadSet = pendingReadIter->second;
            set<unsigned> correspondingRegisters = registerFile.getCorrespondingRegisters(shdw_iter, dstRegs);
            TagManager& tagManager = TagManager::getInstance();

            for(auto corrIter = correspondingRegisters.begin(); corrIter != correspondingRegisters.end(); ++corrIter){
                set<tag_t>& corrTags = pendingUninitializedReads[*corrIter];
                tagManager.decreaseRefCount(corrTags);
                pendingUninitializedReads.erase(*corrIter);
            }

            addPendingRead(correspondingRegisters, pendingReadSet);
        }
    }
}

void addPendingRead(list<REG>* dstRegs, set<tag_t>& tags){
    set<unsigned> shadowRegs;

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    for(auto iter = dstRegs->begin(); iter != dstRegs->end(); ++iter){
        if(!registerFile.isUnknownRegister(*iter)){
            unsigned shdwReg = registerFile.getShadowRegister(*iter);
            shadowRegs.insert(shdwReg);
        }
    }

    addPendingRead(shadowRegs, tags);
}

void updatePendingReads(list<REG>* dstRegs){
    if(pendingUninitializedReads.size() == 0)
        return;

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    for(auto i = dstRegs->begin(); i != dstRegs->end(); ++i){
        if(registerFile.isUnknownRegister(*i))
            continue;
        
        unsigned reg = registerFile.getShadowRegister(*i);
        pendingUninitializedReads.erase(reg);
    }
}






// Next section is related to functions designed to manage pending reads that have been stored
// into memory


/*
    This function tries to merge subsequent ranges that have the very same tag set associated.
    Note that it expects the map passed as a parameter to NOT HAVE OVERLAPPING RANGES.
*/
#define ITERATOR map<range_t, set<tag_t>, IncreasingStartRangeSorter>::iterator
#define GENERIC_ITERATOR typename map<range_t, set<tag_t>, S>::iterator

template <class S>
static GENERIC_ITERATOR next(GENERIC_ITERATOR& iter){
    GENERIC_ITERATOR ret = iter;
    return ++ret;
}

static map<range_t, set<tag_t>, IncreasingStartRangeSorter>& merge(map<range_t, set<tag_t>, IncreasingStartRangeSorter>& m){
    if(m.size() <= 1)
        return m;
    
    bool isPendingReadMap = &m == &storedPendingUninitializedReads;
    auto iter = m.begin(); 
    TagManager& tagManager = TagManager::getInstance();
    auto nxt = next<IncreasingStartRangeSorter>(iter);
    while(nxt != m.end()){
        range_t r1 = iter->first;
        range_t r2 = nxt->first;

        set<tag_t>& s1 = m[r1];
        set<tag_t>& s2 = m[r2];

        // if ranges are contigous and contain the same set of tags, merge them
        if(r1.second + 1 == r2.first && s1 == s2){
            range_t newRange(r1.first, r2.second);
            pair<range_t, set<tag_t>> newElem(newRange, s1);
            pair<ITERATOR, bool> ret = m.insert(newElem);
            m.erase(iter);
            m.erase(nxt);
            if(isPendingReadMap)
                tagManager.decreaseRefCount(s2);
            iter = ret.first;
        }
        else{
            ++iter;
        }
        nxt = next<IncreasingStartRangeSorter>(iter);
    }

    return m;
}

/*
    Splits |iter| inside |storedPendingUninitializedReads| in correspondence of |splittingRange|. 
    The splitted ranges will have the same tag set of the original range, while the newly created range (|splittingRange|)
    will have the tag set |newTags|.

    @param iter: Iterator to the element whose range is going to be splitted
    @param splittingRange:  Range in correspondence of which the range of |iter| is going to be splitted.
                            After the split, ranges preceding and following |splittingRange| (if any) will have the original tag set stored by |iter|;
                            range |splittingRange|, instead, stores |newTags|.
    @param newTags: New tag set to be assigned to range |splittingRange|

    @return Iterator to the first splitted range
*/
static ITERATOR split(ITERATOR& iter, range_t& splittingRange, set<tag_t>& newTags){
    ITERATOR ret;
    bool retValueSet = false;
    TagManager& tagManager = TagManager::getInstance();

    const range_t& curr_range = iter->first;
    const set<tag_t>& origTags = iter->second;
    unsigned origRefCount = 1;

    if(splittingRange == curr_range){
        storedPendingUninitializedReads[curr_range] = newTags;
        tagManager.decreaseRefCount(origTags);
        tagManager.increaseRefCount(newTags);
        return iter;
    }

    if(splittingRange.first > curr_range.first){
        range_t newRange(curr_range.first, splittingRange.first - 1);
        pair<range_t, set<tag_t>> newEl(newRange, origTags);
        pair<ITERATOR, bool> response = storedPendingUninitializedReads.insert(newEl);
        ret = response.first;
        retValueSet = true;
    }
    else{
        --origRefCount;
    }

    /*
        If there is no range before the splitting range, return the iterator to the splitting range
    */
    if(!retValueSet){
        pair<range_t, set<tag_t>> newEl(splittingRange, newTags);
        pair<ITERATOR, bool> response = storedPendingUninitializedReads.insert(newEl);
        ret = response.first;
    }
    // Otherwise just store the splitting range
    else{
        storedPendingUninitializedReads[splittingRange] = newTags;
    }

    tagManager.increaseRefCount(newTags);

    if(splittingRange.second < curr_range.second){
        range_t newRange(splittingRange.second + 1, curr_range.second);
        storedPendingUninitializedReads[newRange] = origTags;
        ++origRefCount;
    }

    if(origRefCount == 0)
        tagManager.decreaseRefCount(origTags);

    if(origRefCount > 1){
        for(unsigned i = 0; i < origRefCount - 1; ++i){
            tagManager.increaseRefCount(origTags);
        }
    }

    storedPendingUninitializedReads.erase(iter);
    return ret;
}


/*
    Splits |iter| inside |m| in correspondence of |splittingRange|. 
    The splitted ranges will have the same tag set of the original range, while the range (|splittingRange|)
    will simply be removed.

    @param m: Map where ranges have to be splitted and erased
    @param iter: Iterator to the element whose range is going to be splitted
    @param splittingRange:  Range in correspondence of which the range of |iter| is going to be splitted.
                            After the split, ranges preceding and following |splittingRange| (if any) will have the original tag set stored by |iter|;
                            range |splittingRange|, instead, will be removed.

    @return Iterator to the first splitted range
*/
template <class S>
static GENERIC_ITERATOR split(map<range_t, set<tag_t>, S>& m, GENERIC_ITERATOR& iter, range_t& splittingRange){
    GENERIC_ITERATOR ret = next<S>(iter);
    bool retValueSet = false;
    bool isPendingReadMap = (void*)&m == (void*)&storedPendingUninitializedReads;

    const range_t& curr_range = iter->first;
    const set<tag_t>& origTags = iter->second;
    TagManager& tagManager = TagManager::getInstance();
    unsigned origRefCount = 1;

    if(splittingRange == curr_range){
        m.erase(iter);
        if(isPendingReadMap)
            tagManager.decreaseRefCount(origTags);
        return ret;
    }

    if(splittingRange.first > curr_range.first){
        range_t newRange(curr_range.first, splittingRange.first - 1);
        pair<range_t, set<tag_t>> newEl(newRange, origTags);
        pair<GENERIC_ITERATOR, bool> response = m.insert(newEl);
        ret = response.first;
        retValueSet = true;
    }
    else{
        --origRefCount;
    }

    if(splittingRange.second < curr_range.second){
        range_t newRange(splittingRange.second + 1, curr_range.second);
        pair<range_t, set<tag_t>> newEl(newRange, origTags);
        pair<GENERIC_ITERATOR, bool> response = m.insert(newEl);
        if(!retValueSet){
            ret = response.first;
        }
        ++origRefCount;
    }

    if(isPendingReadMap){
        if(origRefCount == 0)
            tagManager.decreaseRefCount(origTags);

        if(origRefCount > 1){
            for(unsigned i = 0; i < origRefCount - 1; ++i){
                tagManager.increaseRefCount(origTags);
            }
        }
    }

    m.erase(iter);
    return ret;    
}

#undef GENERIC_ITERATOR
#undef ITERATOR

static range_t getOverlappingRange(const range_t& r1, const range_t& r2){
    ADDRINT end = std::min(r1.second, r2.second);
    ADDRINT start = std::max(r1.first, r2.first);

    return range_t(start, end);
}

static void insertStoredPendingReads(map<range_t, set<tag_t>, IncreasingStartRangeSorter>& m){
    map<range_t, set<tag_t>, IncreasingEndRangeSorter> endRangeSorted;
    endRangeSorted.insert(m.begin(), m.end());

    // Update overlapping ranges
    for(auto iter = storedPendingUninitializedReads.begin(); iter != storedPendingUninitializedReads.end(); ++iter){
        /*
            All the next mIter objects will hae the end point of the range bigger than the start point of the range of iter
        */
        auto mIter = endRangeSorted.lower_bound(range_t(0, iter->first.first));

        while(mIter != endRangeSorted.end()){
            // If the tag sets are the same, it's useless to update the map
            // Note that the ranges overlap only if it's true that mIter's range start point is not bigger than iter's range end point;
            if(mIter->first.first <= iter->first.second && mIter->second != iter->second){
                range_t overlappingRange = getOverlappingRange(mIter->first, iter->first);
                iter = split(iter, overlappingRange, mIter->second);
                mIter = split(endRangeSorted, mIter, overlappingRange);
            }
            else{
                ++mIter;
            }
        }
    }

    TagManager& tagManager = TagManager::getInstance();
    // Add new ranges: endRangeSorted now contains only fresh new ranges to be inserted
    for(auto iter = endRangeSorted.begin(); iter != endRangeSorted.end(); ++iter){
        const range_t& currRange = iter->first;
        const set<tag_t>& tags = iter->second;

        storedPendingUninitializedReads[currRange] = tags;
        tagManager.increaseRefCount(tags);
    }

    // New ranges and overlapping ranges may be contigous. Try to merge the map
    merge(storedPendingUninitializedReads);
}

static void removeStoredPendingReads(range_t r){
    auto iter = storedPendingUninitializedReads.begin();
    while (iter != storedPendingUninitializedReads.end()){
        const range_t iterRange = iter->first;

        if(r.second >= iterRange.first && r.first <= iterRange.second){
            range_t overlappingRange = getOverlappingRange(r, iterRange);
            iter = split(storedPendingUninitializedReads, iter, overlappingRange);
        }
        else{
            ++iter;
        }
    }
}


void updateStoredPendingReads(const AccessIndex& ai){
    if(storedPendingUninitializedReads.size() != 0){
        ADDRINT addr = ai.getFirst();
        range_t r(addr, addr + ai.getSecond() - 1);
        removeStoredPendingReads(r);
    }
}


void storePendingReads(list<REG>* srcRegs, MemoryAccess& ma){
    // Remove all ranges overlapping the given MemoryAccess. If there are uninitialized src registers
    // the correct uninitialized ranges will be inserted again
    if(storedPendingUninitializedReads.size() != 0){
        removeStoredPendingReads(range_t(ma.getAddress(), ma.getAddress() + ma.getSize() - 1));
    }
    // If there are no src registers (an immediate is stored) everything is set as initialized, so there's nothing more to do
    if(srcRegs == NULL){
        return;
    }

    ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
    map<range_t, set<tag_t>, IncreasingStartRangeSorter> tmpMap;
    ADDRINT addr = ma.getAddress();

    for(auto iter = srcRegs->begin(); iter != srcRegs->end(); ++iter){

        if(!registerFile.isUninitialized(*iter)){
            continue;
        }

        /*
        The following set will contain all registers (also their sub-registers) whose associated pending reads will be stored in increasing order
        according to their byte size.
    */
        set<unsigned, ShadowRegisterFile::IncreasingSizeRegisterSorter> toCheck;

        unsigned shadowReg = registerFile.getShadowRegister(*iter);
        if(!registerFile.isUnknownRegister(*iter))
            toCheck.insert(shadowReg);

        unsigned byteSize = registerFile.getByteSize(*iter);
        set<unsigned>& aliasingRegisters = registerFile.getAliasingRegisters(*iter);
        for(auto aliasIter = aliasingRegisters.begin(); aliasIter != aliasingRegisters.end(); ++aliasIter){
            if(registerFile.getByteSize((SHDW_REG) *aliasIter) < byteSize)
                toCheck.insert(*aliasIter);
        }

        unsigned previousRegSize = 0;
        ADDRINT maHighAddr = addr + ma.getSize() - 1;
        for(auto subRegsIter = toCheck.begin(); subRegsIter != toCheck.end(); ++subRegsIter){
            if(!registerFile.isUninitialized((SHDW_REG) *subRegsIter)){
                continue;
            }
            
            unsigned regByteSize = registerFile.getByteSize((SHDW_REG)*subRegsIter);
            bool isHighByte = registerFile.isHighByteReg((SHDW_REG) *subRegsIter);
            ADDRINT start = isHighByte ? addr + 1 : addr + previousRegSize;
            ADDRINT end = isHighByte ? addr + 1 : addr + regByteSize - 1;
            // If we are writing only 1 byte to memory and it comes from a high byte register, fix start and end point 
            // of the memory range
            if(isHighByte && ma.getSize() == 1){
                --start;
                --end;
            }

            // In case the register size is higher than the written memory, the range should not go beyond the written memory location.
            // Adjust the end point of the range in order to avoid writing something else
            if(end > maHighAddr)
                end = maHighAddr;

            previousRegSize = isHighByte ? previousRegSize + regByteSize : regByteSize;
            /*
                This irregular range may happen on those registers having a high byte register.
                If both the high byte register and the low byte registers are uninitialized, the content of the register of
                size 2 is determined by those 2 sub-registers.
                Moreover, when this happens |previousRegSize| is set to 2 (1 byte for low byte, 1 for high byte registers), so 
                |start| will be |addr| + 2; instead, the end point will be |addr| + 2 - 1, thus generating the irregular range.
                As said, in this case the content of the 2 bytes register is fully determined by its sub-registers.
                However, if the register does not have a high byte register or only one of them is uninitialized, the range for the 2 bytes
                register is added correctly.
            */
            if(start > end)
                continue;
            range_t currRange(start, end);
            set<tag_t>& tags = pendingUninitializedReads[*subRegsIter];
            if(tags.size() == 0)
                continue;
            
            auto tmpMapIter = tmpMap.find(currRange);
            if(tmpMapIter != tmpMap.end()){
                tmpMap[currRange].insert(tags.begin(), tags.end());
            }
            else{
                tmpMap[currRange] = tags;
            }
        }
    }

    if(tmpMap.size() == 0)
        return;

    merge(tmpMap);
    insertStoredPendingReads(tmpMap);
}


map<range_t, set<tag_t>> getStoredPendingReads(MemoryAccess& ma){
    ADDRINT addr = ma.getAddress();
    UINT32 size = ma.getSize();
    AccessIndex ai(addr, size);
    return getStoredPendingReads(ai);
}

map<range_t, set<tag_t>> getStoredPendingReads(AccessIndex& ai){
    map<range_t, set<tag_t>> ret;

    ADDRINT addr = ai.getFirst();
    UINT32 size = ai.getSecond();
    range_t memRange(addr, addr + size - 1);

    for(auto iter = storedPendingUninitializedReads.begin(); iter != storedPendingUninitializedReads.end(); ++iter){
        const range_t& iterRange = iter->first;

        if(memRange.first <= iterRange.second && memRange.second >= iterRange.first){
            ret[iter->first] = iter->second;
        }
    }

    return ret;
}

set<range_t, IncreasingStartRangeSorter>& rangeDiff(set<range_t, IncreasingStartRangeSorter>& ranges, const range_t& r2){
    auto iter = ranges.begin();
    while(iter != ranges.end()){
        if(r2.first <= iter->second && r2.second >= iter->first){
            range_t overlappingRange = getOverlappingRange(*iter, r2);
            range_t newRange;
            auto newIter = iter;

            if(overlappingRange == *iter){
                ++newIter;
                ranges.erase(iter);
                iter = newIter;
                continue;
            }

            if(overlappingRange.first > iter->first){
                newRange = range_t(iter->first, overlappingRange.first - 1);
                auto ret = ranges.insert(newRange);
                newIter = ret.first;
            }

            if(overlappingRange.second < iter->second){
                newRange = range_t(overlappingRange.second + 1, iter->second);
                auto ret = ranges.insert(newRange);
                if(newIter == iter){
                    newIter = ret.first;
                }
            }

            ranges.erase(iter);
            iter = newIter;
        }
        ++iter;
    }

    return ranges;
}

set<range_t, IncreasingStartRangeSorter>& rangeIntersect(set<range_t, IncreasingStartRangeSorter>& ranges, set<range_t, IncreasingStartRangeSorter>& intersectSet){
    set<range_t, IncreasingStartRangeSorter> ret;

    for(auto iter = ranges.begin(); iter != ranges.end(); ++iter){
        for(auto i = intersectSet.begin(); i != intersectSet.end(); ++i){
            const range_t& r = *i;
            // If current range overlaps |r|, add it to the return map
            if(r.first <= iter->second && r.second >= iter->first){
                range_t overlappingRange = getOverlappingRange(*iter, r);

                ret.insert(overlappingRange);
            }
        }
    }

    ranges.clear();
    ranges.insert(ret.begin(), ret.end());

    return ranges;
}


void copyStoredPendingReads(MemoryAccess& srcMA, MemoryAccess& dstMA, list<REG>* srcRegs){
    map<range_t, set<tag_t>> toCopy = getStoredPendingReads(srcMA);
    map<range_t, set<tag_t>, IncreasingStartRangeSorter> converted;
    ADDRINT srcAddr = srcMA.getAddress();
    ADDRINT dstAddr = dstMA.getAddress();
    set<tag_t> regsTags;

    if(srcRegs != NULL){
        for(auto i = srcRegs->begin(); i != srcRegs->end(); ++i){
            ShadowRegisterFile& registerFile = ShadowRegisterFile::getInstance();
            if(registerFile.isUnknownRegister(*i))
                continue;

            auto regIter = pendingUninitializedReads.find(registerFile.getShadowRegister(*i));
            set<tag_t>& tags = regIter->second;
            regsTags.insert(tags.begin(), tags.end());
        }
    }

    for(auto i = toCopy.begin(); i != toCopy.end(); ++i){
        range_t range = i->first;
        set<tag_t> tagSet = i->second;
        tagSet.insert(regsTags.begin(), regsTags.end());

        range.first = range.first - srcAddr + dstAddr;
        range.second = range.first - srcAddr + dstAddr;
        converted[range] = tagSet;
    }

    range_t dstRange(dstMA.getAddress(), dstMA.getAddress() + dstMA.getSize() - 1);
    if(storedPendingUninitializedReads.size() != 0)
        removeStoredPendingReads(dstRange);

    insertStoredPendingReads(converted);
}
