#include "PGASChecker.h"

using namespace clang;
using namespace ento;

typedef std::unordered_map<int, Handler> defaultHandlers;
defaultHandlers defaults;
routineHandlers handlers;
std::unique_ptr<BuiltinBug> BT;
int barrierTracker = 0; // move into property layer, and add it as a property

// Sample Bug Report
// void reportUnsynchronizedAccess(
//   const CheckerBase* cb, const CallEvent &Call, CheckerContext &C) {

//   ExplodedNode *errorNode = C.generateNonFatalErrorNode();
//   if (!errorNode)
//     return;

//   if (!BT){
//     std::cout<<"1*\n";
//     BT.reset(new BuiltinBug(
//           cb, "Out-of-bound array access",
//           "Access out-of-bound array element (buffer overflow)"));
//   }
//   std::cout<<"2*\n";
//   auto R =
//         std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), errorNode); 
//         std::cout<<"3*\n";
//   R->addRange(Call.getSourceRange()); 
//   std::cout<<"4*\n";
//   C.emitReport(std::move(R));
//   std::cout<<"5*\n";
// }

/**
 * @brief Invoked on allocation of symmetric variable
 *
 * @param handler
 * @param Call
 * @param C
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
    State = Properties::addToUnintializedList(State, ptrRegion);
    // Properties::transformState(C, State);
    // mark is synchronized by default
    State = Properties::markAsSynchronized(State, allocatedVariable);
    // remove the variable from the freed list if allocated again
    State = Properties::removeFromFreeList(State, allocatedVariable);
    // update the program state graph;
    // every time we make a change to the program state we need to invoke the
    // transform state
    State = Properties::addToArrayList(State, ptrRegion);
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
 */
void DefaultHandlers::handleBarriers(int handler, const CallEvent &Call,
                                     CheckerContext &C, const OpenShmemBugReporter* BReporter) {

  ProgramStateRef State = C.getState();
  auto trackedVariables = State->get<CheckerState>();

  switch (handler) {
  case PRE_CALL:
    break;
  case POST_CALL:

    if(barrierTracker == 0){
        // ExplodedNode *errorNode = C.generateNonFatalErrorNode();
        // if (!errorNode) return;

        // if (!BT){
        //   BT.reset(new BuiltinBug(cb, "Barrier Not Needed","This barrier is unnecessary, please consider removing it"));
        // }
        // auto R = std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), errorNode);
        // R->addRange(Call.getSourceRange());
        // C.emitReport(std::move(R));
    }

    barrierTracker = 0;

    for (PGASMapImpl::iterator I = trackedVariables.begin(),
                               E = trackedVariables.end();
         I != E; ++I) {

      SymbolRef symmetricVariable = I->first;
      // mark all symmetric variables as synchronized
      State = Properties::markAsSynchronized(State, symmetricVariable);
      // important to invoke this each time/each variable
      Properties::transformState(C, State);
    }

    llvm::ImmutableMap<const clang::ento::MemRegion*, clang::ento::TrackingClass> map = State->get<RegionTracker>();
    for(llvm::ImmutableMap<const clang::ento::MemRegion*, clang::ento::TrackingClass>::iterator i = map.begin(); i != map.end(); i++){
      // std::cout << "Barrier-X\n";

      const MemRegion* arrayBasePtr = i->first;
      // reset all trackers
      TrackingClass trackingClass;
      State = State->remove<RegionTracker>(arrayBasePtr);
      State = State->set<RegionTracker>(arrayBasePtr, trackingClass);
      Properties::transformState(C, State);
    }
    break;
  }
}

/**
 * @brief Handler for non blocking writes
 *
 * @param handler
 * @param Call
 * @param C
 */
void DefaultHandlers::handleNonBlockingWrites(int handler,
                                              const CallEvent &Call,
                                              CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  ProgramStateRef State = C.getState();
  
  SymbolRef destVariable = Call.getArgSVal(0).getAsSymbol();

  const MemRegion* MR = Call.getArgSVal(0).getAsRegion();
  if(!MR) {
    std::cout << "Failed while getting the Memory Region. Returning!!";
    return;
  }

  const ElementRegion *ER = dyn_cast<ElementRegion>(MR);
  if (!ER){
    std::cout << "Failed while casting into the Element Region. Returning!!";
    return;
  }

  switch (handler) {
  case PRE_CALL:
  {
    if(MR->hasGlobalsOrParametersStorage()){  //Also, use for static storage
        State = Properties::addToArrayList(State, ER->getSuperRegion());
        Properties::transformState(C, State);
    }


    const RefState *SS = State->get<CheckerState>(destVariable);

  }
    break;
  case POST_CALL:

    barrierTracker = 1;
    // remove the unintialized variables
    State = Properties::removeFromUnitializedList(State, destVariable);
    // mark as unsynchronized
    State = Properties::markAsUnsynchronized(State, destVariable);

    if (!ER){
      std::cout << "Not an element region\n";
    } else {

      // Get the array index 
      DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
      SVal numElements = C.getSVal(Call.getArgExpr(2));
      SVal nodeIndex = C.getSVal(Call.getArgExpr(3));

      const MemRegion* parentRegion = ER->getSuperRegion();
      
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
 */
void DefaultHandlers::handleBlockingWrites(int handler, const CallEvent &Call,
                                           CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  ProgramStateRef State = C.getState();
  SymbolRef destVariable = Call.getArgSVal(0).getAsSymbol();

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

// invoked on read routines such as shmem_get
void DefaultHandlers::handleReads(int handler, const CallEvent &Call,
                                  CheckerContext &C, const OpenShmemBugReporter* BReporter) {

  ProgramStateRef State = C.getState();
  SymbolRef symmetricVariable = Call.getArgSVal(0).getAsSymbol();
  const RefState *SS = State->get<CheckerState>(symmetricVariable);
  const MemRegion *const MR = Call.getArgSVal(0).getAsRegion();
  const ElementRegion *const ER = dyn_cast<ElementRegion>(MR);
  
  switch (handler) {

  case PRE_CALL:
    break;

  case POST_CALL:

    if (!ER){
      std::cout << "Not an element region\n";
    } else {

    DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
    SVal num_elements = C.getSVal(Call.getArgExpr(2));
    SVal nodeIndex = C.getSVal(Call.getArgExpr(3));
 
    const MemRegion* parentRegion = ER->getSuperRegion();

    bool result = Properties::checkTrackerRange(C, parentRegion, Idx, num_elements, nodeIndex);
    if(!result){
      BReporter->reportUnsafeRead(C, Call);
      return;
    }
  }
  Properties::transformState(C, State);

    break;
  }

}

/**
 * @brief invoked on memory deallocation routines as shmem_free
 *
 * @param handler
 * @param Call
 * @param C
 */
void DefaultHandlers::handleMemoryDeallocations(int handler,
                                                const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter) {

  ProgramStateRef State = C.getState();
  const MemRegion* freedVariable = Call.getArgSVal(0).getAsRegion()->getBaseRegion();

  switch (handler) {
  case PRE_CALL:
    break;
  case POST_CALL:
    // add it to the freed variable set; since it is adding it multiple times
    // should have the same effect
    State = Properties::addToFreeList(State, freedVariable);
    if(!State){
  //     ExplodedNode *errorNode = C.generateNonFatalErrorNode();
  // if (!errorNode)
  //   return;

  // if (!BT){
  //   BT.reset(new BuiltinBug(
  //         cb, "Double Free",
  //         "Either this memory region was already free or never assigned"));
  // }
  // auto R =
  //       std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), errorNode); 
  // R->addRange(Call.getSourceRange());
  // C.emitReport(std::move(R));
    } else {
      Properties::transformState(C, State);
    }
    // stop tracking freed variable
    State = Properties::removeFromState(State, freedVariable);
    Properties::transformState(C, State);
    break;
  }
}

void DefaultHandlers::handleFinalCalls(int handler,
                                                const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter) {
  switch (handler) {
  case POST_CALL:
  ProgramStateRef State = C.getState();
  bool result = Properties::testMissingFree(State);
  if(result){
        // ExplodedNode *errorNode = C.generateNonFatalErrorNode();
        // if (!BT){
        //   BT.reset(new BuiltinBug( cb, "Missing Free","The memory region was not freed"));
        // }
        // auto R = std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), errorNode); 
        // R->addRange(Call.getSourceRange());
        // C.emitReport(std::move(R));
    }
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
  
  std::cout << "PostCall " << routineName << ":\n ";
  
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