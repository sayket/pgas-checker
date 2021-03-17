#include "PGASChecker.h"

using namespace clang;
using namespace ento;

typedef std::unordered_map<int, Handler> defaultHandlers;
defaultHandlers defaults;
routineHandlers handlers;
int barrierTracker = 0; // TODO: move into property layer, and add it as a property

void perormSynchronization(CheckerContext &C, ProgramStateRef State){
    State = Properties::clearMap(State);
    Properties::transformState(C, State);
}

/**
 * @brief Invoked on allocation of symmetric variable
 *
 * @param handler
 * @param Call
 * @param C
 * @param BReporter
 */
void DefaultHandlers::handleMemoryAllocations(int handler,
                                              const CallEvent &Call,
                                              CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  ProgramStateRef State = C.getState();
  // get the reference to the allocated variable
  SymbolRef allocatedVariable = Call.getReturnValue().getAsSymbol();

  switch (handler) {

  case PRE_CALL:
    break;

  case POST_CALL:

    const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(Call.getDecl());
    if (!FD) return;
    std::string routineName = FD->getNameInfo().getAsString();
    
    const MemRegion* ptrRegion = Call.getReturnValue().getAsRegion();
    
    // add unitilized variables to unitilized list
    State = Properties::recordThisAllocation(State, ptrRegion, C.generateErrorNode());
    
    // mark is synchronized by default
    State = Properties::markAsSynchronized(State, allocatedVariable);
    
    // remove the variable from the freed list if allocated again
    State = Properties::removeFromFreeList(State, allocatedVariable);
    
    // update the program state graph;
    // every time we make a change to the program state we need to invoke the
    // transform state
    State = Properties::addToArrayList(State, ptrRegion);
    perormSynchronization(C, State);
    break;
  }
}

/**
 * @brief invoked on synchronization barriers; this is specifically for
 * barrier_all
 *
 * @param handler
 * @param Call
 * @param C
 * @param BReporter
 */
void DefaultHandlers::handleBarriers(int handler, const CallEvent &Call,
                                     CheckerContext &C, const OpenShmemBugReporter* BReporter) {

  //TODO: Add Extra Barrier Tracker in this function

  ProgramStateRef State = C.getState();

  switch (handler) {
  case PRE_CALL:
    break;
  case POST_CALL:
    perormSynchronization(C, State);
    break;
  }
}

/**
 * @brief Handler for non blocking writes
 *
 * @param handler
 * @param Call
 * @param C
 * @param BReporter
 */
void DefaultHandlers::handleNonBlockingWrites(int handler,
                                              const CallEvent &Call,
                                              CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  
  int memRegionArgIndex = 0, numElementsArgIndex = 2, rankArgIndex = 3;
  ProgramStateRef State = C.getState();
  SymbolRef destVariable = Call.getArgSVal(memRegionArgIndex).getAsSymbol();
  
  const MemRegion* MR = Call.getArgSVal(memRegionArgIndex).getAsRegion();
  if(!MR) {
    std::cout << "Failed while getting the Memory Region. Returning!!\n";
    return;
  }

  const MemRegion* regionToCheck = MR;

  const ElementRegion *ER = dyn_cast<ElementRegion>(MR);
  if (ER){
    // std::cout << "Failed while casting into the Element Region. Returning!!\n";
    regionToCheck = (ER->getSuperRegion());
  }

  // const RegionOffset offSet = ER->getAsOffset();
  // int64_t offsetVal = offSet.getOffset();
  // const MemRegion* regionToCheck = (offsetVal == 0)?ER:(ER->getSuperRegion());

  switch (handler) {
  case PRE_CALL:
  {
    // Checks if the Memory Region is global or static
    if(MR->hasGlobalsOrParametersStorage()){
        State = Properties::addToArrayList(State, regionToCheck);
        Properties::transformState(C, State);
    }

    bool isRegionSymmetric = Properties::isMemRegionSymmetric(State, regionToCheck);
    if(!isRegionSymmetric){
      BReporter->reportUnSymmetricAccess(C, Call);
      // return;
    }


    bool isMemRegionAvailable = Properties::isMemRegionAvailable(State, regionToCheck);
    if(!isMemRegionAvailable){
      BReporter->reportMissingRegion(C, Call);
      return;
    }

  }
    break;
  case POST_CALL:

    barrierTracker = 1;

    // remove the unintialized variables
    State = Properties::removeFromUnitializedList(State, destVariable);
    // mark as unsynchronized

    State = Properties::markAsUnsynchronized(State, destVariable);

    if (!ER){
      // std::cout << "Not an element region\n";
      
      // uint64_t val = 0;
      // unsigned numBits = sizeof(val);

      // llvm::APInt apInt = llvm::APInt(numBits, val);
      // const llvm::APSInt apsInt = llvm::APSInt(apInt);
      // DefinedOrUnknownSVal Idx = clang::ento::nonloc::ConcreteInt(apsInt);

      const NonLoc Idx = C.getSValBuilder().makeArrayIndex(0);
      SVal numElements = C.getSVal(Call.getArgExpr(numElementsArgIndex));
      SVal nodeIndex = C.getSVal(Call.getArgExpr(rankArgIndex));

      const MemRegion* parentRegion = regionToCheck;
      
      State = Properties::taintArray(State, parentRegion, Idx, numElements, nodeIndex);
      Properties::transformState(C, State);
    } else {
      // Get the array index 
      DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
      SVal numElements = C.getSVal(Call.getArgExpr(numElementsArgIndex));
      SVal nodeIndex = C.getSVal(Call.getArgExpr(rankArgIndex));

      const MemRegion* parentRegion = regionToCheck;
      
      State = Properties::taintArray(State, parentRegion, Idx, numElements, nodeIndex);
      Properties::transformState(C, State);
    }

    Properties::transformState(C, State);
    break;
  }
}

/**
 * @brief Handler for blocking writes; same as non blocking writes
 *
 * @param handler
 * @param Call
 * @param C
 * @param BReporter
 */
void DefaultHandlers::handleBlockingWrites(int handler, const CallEvent &Call,
                                           CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  int memRegionArgIndex = 0;
  ProgramStateRef State = C.getState();
  SymbolRef destVariable = Call.getArgSVal(memRegionArgIndex).getAsSymbol();

  switch (handler) {
  case PRE_CALL:
    break;
  case POST_CALL:
    // remove from uninitilized list
    State = Properties::removeFromUnitializedList(State, destVariable);
    Properties::transformState(C, State);
    break;
  }
}

/**
 * @brief invoked on synchronization barriers; this is specifically for
 * barrier_all
 *
 * @param handler
 * @param Call
 * @param C
 * @param BReporter
 */
void DefaultHandlers::handleReads(int handler, const CallEvent &Call,
                                  CheckerContext &C, const OpenShmemBugReporter* BReporter) {

  // symmetric mem region index is 1 (2nd argument)
  int memRegionArgIndex = 1, numElementsArgIndex = 2, rankArgIndex = 3; 
  
  ProgramStateRef State = C.getState();
  const MemRegion *const MR = Call.getArgSVal(memRegionArgIndex).getAsRegion();
  const ElementRegion *const ER = dyn_cast<ElementRegion>(MR);
  int64_t offsetVal = 0;
  // if(ER) {
  //   const RegionOffset offSet = ER->getAsOffset();
  //   offsetVal = offSet.getOffset();
  // }
  // const MemRegion* regionToCheck = (offsetVal == 0)?MR:(ER->getSuperRegion());

  /// If the region is not an element region (has array index)
  const MemRegion* regionToCheck = (!ER)?MR:(ER->getSuperRegion());
  bool isRegionSymmetric = Properties::isMemRegionSymmetric(State, regionToCheck);
  bool isMemRegionAvailable = Properties::isMemRegionAvailable(State, regionToCheck);  
  switch (handler) {

  case PRE_CALL:
    
    if(!isRegionSymmetric){
      BReporter->reportUnSymmetricAccess(C, Call);
      // return;
    }

    if(!isMemRegionAvailable){
      BReporter->reportMissingRegion(C, Call);
      return;
    }

    if (!ER){
      // std::cout << "Not an element region\n";
      
      const NonLoc Idx = C.getSValBuilder().makeArrayIndex(0);
      SVal num_elements = C.getSVal(Call.getArgExpr(numElementsArgIndex));
      SVal nodeIndex = C.getSVal(Call.getArgExpr(rankArgIndex));

      const MemRegion* parentRegion = MR;
      bool result = Properties::checkTrackerRange(C, parentRegion, Idx, num_elements, nodeIndex);
      if(!result){
        BReporter->reportUnsafeRead(C, Call);
        // return;
      }
    } else {
      // uint64_t val = 0;
      // unsigned numBits = sizeof(val);

      // llvm::APInt apInt = llvm::APInt(numBits, val);
      // const llvm::APSInt apsInt = llvm::APSInt(apInt);
      // DefinedOrUnknownSVal Idx = clang::ento::nonloc::ConcreteInt(apsInt);

      DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
      SVal num_elements = C.getSVal(Call.getArgExpr(numElementsArgIndex));
      SVal nodeIndex = C.getSVal(Call.getArgExpr(rankArgIndex));

      const MemRegion* parentRegion = ER->getSuperRegion();
      bool result = Properties::checkTrackerRange(C, parentRegion, Idx, num_elements, nodeIndex);
      if(!result){
        BReporter->reportUnsafeRead(C, Call);
        // return;
      }
    }
    
    Properties::transformState(C, State);

    break;

  case POST_CALL:
    break;

  }

}

/**
 * @brief invoked on memory deallocation routines as shmem_free
 *
 * @param handler
 * @param Call
 * @param C
 * @param BReporter
 */
void DefaultHandlers::handleMemoryDeallocations(int handler,
                                                const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  int memRegionArgIndex = 0;
  ProgramStateRef State = C.getState();
  const MemRegion* freedVariable = Call.getArgSVal(memRegionArgIndex).getAsRegion()->getBaseRegion();

  switch (handler) {
  case PRE_CALL:
    break;
  case POST_CALL:
    // add it to the freed variable set; since it is adding it multiple times
    // should have the same effect
    State = Properties::freeThisAllocation(State, freedVariable);
    if(!State){
      BReporter->reportDoubleFree(C, Call);
      // return;
    } else {
      Properties::transformState(C, State);
    }
    // stop tracking freed variable
    State = Properties::removeFromState(State, freedVariable);
    Properties::transformState(C, State);
    break;
  }
}


/**
 * @brief invoked when the ending shmem routines as called like shmem_finalize
 *
 * @param handler
 * @param Call
 * @param C
 * @param BReporter
 */
void DefaultHandlers::handleFinalCalls(int handler,
                                                const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  switch (handler) {

  case PRE_CALL:
    ProgramStateRef State = C.getState();
    bool result = Properties::testMissingFree(State);
    if(result){
        RangeClass rangeClass = Properties::getMissingFreeAllocation(State);
        BReporter->reportNoFree(C, rangeClass.getErrorNode());
        // return;
    }
    break;
  }
}

/**
 * @brief Construct a new PGASChecker::PGASChecker object
 * Does two things:
 * Get the library specific handlers, populate default handlers
 *
 * @param addHandlers
 */
PGASChecker::PGASChecker(routineHandlers (*addHandlers)()):BReporter(*this){
  handlers = addHandlers();
  addDefaultHandlers();
}

/**
 * @brief Populate the default handlers for each routine type
 *
 */
void PGASChecker::addDefaultHandlers() {
  defaults.emplace(MEMORY_ALLOC, DefaultHandlers::handleMemoryAllocations);
  defaults.emplace(MEMORY_DEALLOC, DefaultHandlers::handleMemoryDeallocations);
  defaults.emplace(SYNCHRONIZATION, DefaultHandlers::handleBarriers);
  defaults.emplace(NON_BLOCKING_WRITE,
                   DefaultHandlers::handleNonBlockingWrites);
  defaults.emplace(READ_FROM_MEMORY, DefaultHandlers::handleReads);
  defaults.emplace(FINAL_CALL, DefaultHandlers::handleFinalCalls);
}

/**
 * @brief retrieve the  default routine handler for a routine type
 *
 * @param routineType
 * @return Handler
 */
Handler PGASChecker::getDefaultHandler(Routine routineType) const {

  defaultHandlers::const_iterator iterator = defaults.find(routineType);

  if (iterator != defaults.end()) {
    return iterator->second;
  }

  std::cout << "Null Handler :P \n";

  return (Handler)NULL;
}

/**
 * @brief Event handler takes in the routine name,
 * and maps the routine name to routine
 * type and the corresponding routine handler of the specific routine
 * invoked handler can be default one or the programming model specific one
 *
 * @param handler
 * @param routineName
 * @param Call
 * @param C
 */
void PGASChecker::eventHandler(int handler, std::string &routineName,
                               const CallEvent &Call, CheckerContext &C) const {
  Handler routineHandler = NULL;
  // get the corresponding iterator to the key
  routineHandlers::const_iterator iterator = handlers.find(routineName);

  if (iterator != handlers.end()) {

    // corresponding <routineType, routineHandler> pair
    Pair value = iterator->second;
    // value.first is the routineType and value.second is the routineHandler
    Routine routineType = value.first;

    // if the event handler exists invoke it; else call the default
    // implementation
    if (value.second) {
      routineHandler = value.second;
    } else {
      routineHandler = getDefaultHandler(routineType);
    }

    // finally invoke the routine handler
    if (routineHandler != NULL) {
      routineHandler(handler, Call, C, &(BReporter));
    } else {
      std::cout << "No implementation found for this routine\n";
    }
  }
}

/**
 * @brief Gets called as a post callback on invocation of a routine
 *
 * @param Call
 * @param C
 */
void PGASChecker::checkPostCall(const CallEvent &Call,
                                CheckerContext &C) const {
  // get the declaration of the invoked routine
  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(Call.getDecl());

  if (!FD) return;

  // get the invoked routine name
  std::string routineName = FD->getNameInfo().getAsString();
  
  // invoke the event handler to figure out the right implementation
  eventHandler(POST_CALL, routineName, Call, C);

}
// pre call callback
void PGASChecker::checkPreCall(const CallEvent &Call, CheckerContext &C) const {

  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(Call.getDecl());

  if (!FD)
    return;

  // get the name of the invoked routine
  std::string routineName = FD->getNameInfo().getAsString();
  
  eventHandler(PRE_CALL, routineName, Call, C);

}