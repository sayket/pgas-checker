#include "PGASChecker.h"

/**
 * @brief every time we make a change to the program state we need to invoke the
 * transform state
 *
 * @param C
 * @param State
 */
void Properties::transformState(CheckerContext &C, ProgramStateRef State) {
  C.addTransition(State);
}

/**
 * @brief
 * remember program state is an immutable data structure, so it is neccessary to
 * return the new state with effect to the caller
 * the caller eventually invokes the transformState to add the new state to the
 * program state graph remove from the uninitialized list
 * @param State
 * @param variable
 * @return ProgramStateRef
 */
ProgramStateRef Properties::removeFromUnitializedList(ProgramStateRef State,
                                                      SymbolRef variable) {
  if (State->contains<UnintializedVariables>(variable)) {
    State = State->remove<UnintializedVariables>(variable);
  } else {
    // TODO: Add the else if applicable
  }
  return State;
}

/**
 * @brief
 *
 * @param State
 * @param variable
 * @return ProgramStateRef
 */
//TODO: Change the function names
ProgramStateRef Properties::removeFromFreeList(ProgramStateRef State,
                                               SymbolRef variable) {
  if (State->contains<FreedVariables>(variable)) {
    State = State->remove<FreedVariables>(variable);
  } else {
    // TODO: Add the else if applicable
  }
  return State;
}

/**
 * @brief add to the free list
 *
 * @param State
 * @param variable
 * @return ProgramStateRef
 */
ProgramStateRef Properties::addToFreeList(ProgramStateRef State,
                                          const MemRegion* arrayRegion) {
  const int64_t* val = (State->get<AllocationTracker>(arrayRegion));
  int64_t result = (*val);
    if(result == 0){
      return NULL;
    }
  State = State->set<AllocationTracker>(arrayRegion, 0);
  return State;
}

ProgramStateRef Properties::addToUnintializedList(ProgramStateRef State,
                                                  const MemRegion* arrayRegion) {
  State = State->set<AllocationTracker>(arrayRegion, 1);
  return State;
}

/**
 * @brief remove the variable program state
 *
 * @param State
 * @param variable
 * @return ProgramStateRef
 */
ProgramStateRef Properties::removeFromState(ProgramStateRef State,
                                            const MemRegion* arrayRegion) {
  // TODO: Remove this function if no longer valid
  return State;
}

/**
 * @brief mark as unsynchronized
 *
 * @param State
 * @param variable
 * @return ProgramStateRef
 */
ProgramStateRef Properties::markAsUnsynchronized(ProgramStateRef State,
                                                 SymbolRef variable) {
  State = State->set<CheckerState>(variable, RefState::getUnsynchronized());
  return State;
}

/**
 * @brief
 *
 * @param State
 * @param variable
 * @return ProgramStateRef
 */
ProgramStateRef Properties::markAsSynchronized(ProgramStateRef State,
                                               SymbolRef variable) {
  State = State->set<CheckerState>(variable, RefState::getSynchronized());
  return State;
}

ProgramStateRef Properties::addToArrayList(ProgramStateRef State,
                                          const MemRegion* arrayRegion) {
  
  State = State->set<AllocationTracker>(arrayRegion, 1);

  TrackingClass t1;
  State = State->set<RegionTracker>(arrayRegion, t1);
  int count = 0;
  llvm::ImmutableMap<const clang::ento::MemRegion*, clang::ento::TrackingClass> map = State->get<RegionTracker>();
  for(llvm::ImmutableMap<const clang::ento::MemRegion*, clang::ento::TrackingClass>::iterator i = map.begin(); i != map.end(); i++){
    count++;
  }
  return State;
}

ProgramStateRef Properties::taintArray(ProgramStateRef State,
                                          const MemRegion* arrayRegion, SVal startIndex, SVal numElements, SVal nodeIndex) {
  if((State->get<RegionTracker>()).contains(arrayRegion)){
    const TrackingClass *tracker = State->get<RegionTracker>(arrayRegion);
    if(tracker){
      TrackingClass trackingClass;
      DefinedOrUnknownSVal startIndex2 = startIndex.castAs<DefinedOrUnknownSVal>();
      DefinedOrUnknownSVal numElements2 = numElements.castAs<DefinedOrUnknownSVal>();
      DefinedOrUnknownSVal nodeIndex2 = nodeIndex.castAs<DefinedOrUnknownSVal>();

      trackingClass.updateTracker(startIndex2, numElements2, nodeIndex2);
      State = State->remove<RegionTracker>(arrayRegion);
      State = State->set<RegionTracker>(arrayRegion, trackingClass);
      return State;
    } else {
      std::cout << "Can't find the tracker\n";
    }
  } else {
    std::cout << "This region is not in list\n";    
  }
  return State;
}

bool Properties::checkTrackerRange(CheckerContext &C,
                                          const MemRegion* arrayRegion, SVal startIndex, SVal numElements, SVal nodeIndex) {
  
  ProgramStateRef State = C.getState();
  if(State->contains<RegionTracker>(arrayRegion)){
    const TrackingClass *tracker = State->get<RegionTracker>(arrayRegion);
    if(tracker){
      DefinedOrUnknownSVal startIndex2 = startIndex.castAs<DefinedOrUnknownSVal>();
      DefinedOrUnknownSVal numElements2 = numElements.castAs<DefinedOrUnknownSVal>();
       DefinedOrUnknownSVal nodeIndex2 = nodeIndex.castAs<DefinedOrUnknownSVal>();

      bool flag = (*tracker).isRangeEmpty(startIndex2, numElements2, nodeIndex2, C);
      return flag;
    } else {
      std::cout << "Can't find the tracker\n";
    }
  } else {
    std::cout << "This region is not in list\n";    
  }
  return false;
  
}

bool Properties::testMissingFree(ProgramStateRef State){
  auto it = (State->get<AllocationTracker>()).begin();
  for(;it != (State->get<AllocationTracker>()).end(); ++it){
    if((it->second) == 1){
        return true;
    }
  }
  return false;
}

bool Properties::regionExistsInMap(ProgramStateRef State,
                                          const MemRegion* arrayRegion){
  return State->contains<RegionTracker>(arrayRegion);

}

void TrackingClass::updateTracker(DefinedOrUnknownSVal startIndex, DefinedOrUnknownSVal numElements, DefinedOrUnknownSVal nodeIndex) {

    std::pair<DefinedOrUnknownSVal, DefinedOrUnknownSVal> p1 = std::make_pair(startIndex, numElements);
    
    if(startIndex.isUnknownOrUndef()){
      std::cout << "Start Index has an unknown or undefined SVal \n";
    }

    if(numElements.isUnknownOrUndef()){
      std::cout << "Number of elements has an unknown or undefined SVal \n";
    }

    const int64_t index = nodeIndex.castAs<nonloc::ConcreteInt>().getValue().getExtValue();

    p1.first = startIndex;
    p1.second = numElements;

    auto it = trackingMap.find(index);
    trackingVector tV;
    if(it == trackingMap.end()){
      trackingMap[index] = tV;
    }
    tV = trackingMap[index];
    tV.push_back(p1);
    trackingMap[index] = tV;
  }

  bool TrackingClass::isRangeEmpty(DefinedOrUnknownSVal startIndex, DefinedOrUnknownSVal numElements, DefinedOrUnknownSVal nodeIndex, CheckerContext &C) const{

    ProgramStateRef state = C.getState();
    SValBuilder &svalBuilder = C.getSValBuilder();
    
    if(startIndex.isUnknownOrUndef()){
      std::cout << "Start Index has an unknown or undefined SVal \n";
    }

    if(numElements.isUnknownOrUndef()){
      std::cout << "Number of elements has an unknown or undefined SVal \n";
    }

    const int64_t index = nodeIndex.castAs<nonloc::ConcreteInt>().getValue().getExtValue();

    auto it1 = trackingMap.find(index);
    if(it1 == trackingMap.end()){
      std::cout << "Didn't find the tracking vector\n";
      return true;
    }
    trackingVector tV = it1->second;

    DefinedOrUnknownSVal endIndex = svalBuilder.evalBinOp(state, BO_Add, startIndex, numElements, svalBuilder.getArrayIndexType()).castAs<DefinedOrUnknownSVal>();
    endIndex = svalBuilder.evalBinOp(state, BO_Sub, endIndex, svalBuilder.makeArrayIndex(1), svalBuilder.getArrayIndexType()).castAs<DefinedOrUnknownSVal>();
    auto it = tV.begin();
    for(; it != tV.end(); ++it){
          DefinedOrUnknownSVal startPoint = (*it).first;
          DefinedOrUnknownSVal numElements2 = (*it).second;
          DefinedOrUnknownSVal endPoint = svalBuilder.evalBinOp(state, BO_Add, startPoint, numElements2, svalBuilder.getArrayIndexType()).castAs<DefinedOrUnknownSVal>();
          endPoint = svalBuilder.evalBinOp(state, BO_Sub, endPoint, svalBuilder.makeArrayIndex(1), svalBuilder.getArrayIndexType()).castAs<DefinedOrUnknownSVal>();
    
          ProgramStateRef toRightof =  state->assumeInBound(startIndex, endPoint, false);
          ProgramStateRef toLeftof =  state->assumeInBound(endIndex, startPoint, true);

          ProgramStateRef toRightof2 =  state->assumeInBound(startPoint, endIndex, false);
          ProgramStateRef toLeftof2 =  state->assumeInBound(endPoint, startIndex, true);

          if(!toLeftof && !toRightof) return false;
          if(!toLeftof2 && !toRightof2) return false;
    }

    return true;
  }

