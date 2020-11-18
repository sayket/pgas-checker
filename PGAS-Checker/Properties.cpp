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
    // std::cout << "RU:It contains the variable\n";
    State = State->remove<UnintializedVariables>(variable);
  } else {
    // std::cout << "RU:It doesn't contain the variable\n";
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
ProgramStateRef Properties::removeFromFreeList(ProgramStateRef State,
                                               SymbolRef variable) {
  if (State->contains<FreedVariables>(variable)) {
    // std::cout << "RF:It contains the variable\n";
    State = State->remove<FreedVariables>(variable);
  } else {
    // std::cout << "RF:It doesn't contains the variable\n";
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
  // State = State->add<UnintializedVariables>(variable);
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
  // const RefState *SS = State->get<CheckerState>(variable);
  // if (SS) {
  //   State = State->remove<CheckerState>(variable);
  // }
  // if((State->get<AllocationTracker>()).contains(arrayRegion)){
  //   State = State->remove<AllocationTracker>(arrayRegion);
  // }
  // const RefState *SS = State->get<CheckerState>(variable);
  // if (SS) {
  //   State = State->remove<CheckerState>(variable);
  // }
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
  
  // if((State->get<AllocationTracker>()).contains(arrayRegion)){
  //   int64_t val = *(State->get<AllocationTracker>(arrayRegion));
  //   if(val == 1) return NULL;
  // }

  State = State->set<AllocationTracker>(arrayRegion, 1);

  // auto it = (State->get<AllocationTracker>()).begin();
  // for(;it != (State->get<AllocationTracker>()).end(); ++it){
  //   std::cout << (it->first)->getString() << "," << it->second << "\n";
  // }

  // int64_t val = *(State->get<AllocationTracker>(arrayRegion));
  // //   if(val == 0){
  //     std::cout<<" Val Entered Properly " << val << "\n";
  // //     return NULL;
  // //   }
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
    // std::cout << "This region is there\n";
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

