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
                                          SymbolRef variable) {
  State = State->add<FreedVariables>(variable);
  return State;
}

ProgramStateRef Properties::addToUnintializedList(ProgramStateRef State,
                                                  SymbolRef variable) {
  State = State->add<UnintializedVariables>(variable);
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
                                            SymbolRef variable) {
  const RefState *SS = State->get<CheckerState>(variable);
  if (SS) {
    State = State->remove<CheckerState>(variable);
  }
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
  TrackingClass t1;
  State = State->set<RegionTracker>(arrayRegion, t1);
  std::cout << "Region added: " << arrayRegion << "\n";
  int count = 0;
  llvm::ImmutableMap<const clang::ento::MemRegion*, clang::ento::TrackingClass> map = State->get<RegionTracker>();
  for(llvm::ImmutableMap<const clang::ento::MemRegion*, clang::ento::TrackingClass>::iterator i = map.begin(); i != map.end(); i++){
    std::cout << "Incr count\n";
    count++;
  }
  std::cout << "Size: " << count << "\n";
  return State;
}

ProgramStateRef Properties::taintArray(ProgramStateRef State,
                                          const MemRegion* arrayRegion, SVal startIndex, SVal numElements, SVal nodeIndex) {
  std::cout << "Region checking: " << arrayRegion << "\n";
  if((State->get<RegionTracker>()).contains(arrayRegion)){
    std::cout << "This region is there\n";
    const TrackingClass *tracker = State->get<RegionTracker>(arrayRegion);
    if(tracker){
      // std::cout << "Node Index: " << nodeIndex << "\n";
      TrackingClass trackingClass;
      // trackingClass.trackingMap = (*tracker).trackingMap;
      DefinedOrUnknownSVal startIndex2 = startIndex.castAs<DefinedOrUnknownSVal>();
      DefinedOrUnknownSVal numElements2 = numElements.castAs<DefinedOrUnknownSVal>();
      DefinedOrUnknownSVal nodeIndex2 = nodeIndex.castAs<DefinedOrUnknownSVal>();

      trackingClass.updateTracker(startIndex2, numElements2, nodeIndex2);
      // std::cout << "New tracker: " << trackingClass.t1 << "\n";
      State = State->remove<RegionTracker>(arrayRegion);
      State = State->set<RegionTracker>(arrayRegion, trackingClass);
      // const TrackingClass *tracker2 = State->get<RegionTracker>(arrayRegion);
      // std::cout << "Just to make (nc) sure: " << (*tracker2).t1 << "\n";
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
    std::cout << "This region is there\n";
    const TrackingClass *tracker = State->get<RegionTracker>(arrayRegion);
    if(tracker){
      DefinedOrUnknownSVal startIndex2 = startIndex.castAs<DefinedOrUnknownSVal>();
      DefinedOrUnknownSVal numElements2 = numElements.castAs<DefinedOrUnknownSVal>();
       DefinedOrUnknownSVal nodeIndex2 = nodeIndex.castAs<DefinedOrUnknownSVal>();

      bool flag = (*tracker).isRangeEmpty(startIndex2, numElements2, nodeIndex2, C);
      // std::cout << "Tracker Empty: " << flag << "\n";
      return flag;
    } else {
      std::cout << "Can't find the tracker\n";
    }
  } else {
    std::cout << "This region is not in list\n";    
  }
  return false;
  
}

// void Properties::printTheMap(ProgramStateRef State){
//       /// TEST START /////

//     std::cout << "Print function has started \n";

//     auto trMap = State->get<RegionTracker>();    
    
//     for (RegionTrackerTy::iterator I = trMap.begin(),
//                                E = trMap.end();
//          I != E; ++I) {
//       // std::cout << "Loop iterator: \n";
//       const MemRegion* arrayBasePtr = I->first;
//       // reset all trackers
//       const TrackingClass *tracker = State->get<RegionTracker>(arrayBasePtr);
//       std::cout << "tracker-iters: " << (*tracker).trackingMap << "\n";
//     }
//     /// TEST END /////
// }

bool Properties::regionExistsInMap(ProgramStateRef State,
                                          const MemRegion* arrayRegion){
  return State->contains<RegionTracker>(arrayRegion);

}

