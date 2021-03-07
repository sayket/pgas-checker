#include "OpenShmemChecker.h"

using namespace clang;
using namespace ento;

namespace clang{
namespace ento{

void perormSynchronization(CheckerContext &C, ProgramStateRef State){
    State = Properties::clearMap(State);
    Properties::transformState(C, State);
}

void handleMemoryReallocations(int handler, const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter){

  int memRegionArgIndex = 0, memRegionSizeIndex = 1;
  ProgramStateRef State = C.getState();
  const MemRegion* reAllocVariable = Call.getArgSVal(memRegionArgIndex).getAsRegion()->getBaseRegion();
  const SVal memRegionSize = C.getSVal(Call.getArgExpr(memRegionSizeIndex));

  switch (handler) {

  case PRE_CALL:

  {
    bool isRegionSymmetric = Properties::isMemRegionSymmetric(State, reAllocVariable);
    if(!isRegionSymmetric){
        BReporter->reportUnSymmetricAccess(C, Call);
        return;
    }

    bool isReallocEligible = Properties::isEligibleForRealloc(State, reAllocVariable);
    if(!isReallocEligible){
        BReporter->reportInvalidReallocation(C, Call);
        return;
    }    

    bool isSizeValid = Properties::isArgNonNegative(memRegionSize);
    if(!isSizeValid){
        BReporter->reportInvalidSizeEntry(C, Call);
        return;
    }
  }
    break;

  case POST_CALL:
    {
    const MemRegion* ptrRegion = Call.getReturnValue().getAsRegion();
    State = Properties::recordThisAllocation(State, ptrRegion, C.generateErrorNode());
    
    State = Properties::addToArrayList(State, ptrRegion);
    perormSynchronization(C, State);    }
    break;
  }
}

void handleMemoryAlignments(int handler, const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter){

  int memAlignmentIndex = 0, memRegionSizeIndex = 1;

  ProgramStateRef State = C.getState();
  const SVal memRegionSize = C.getSVal(Call.getArgExpr(memRegionSizeIndex));
  const SVal alignmentVal = C.getSVal(Call.getArgExpr(memAlignmentIndex));

  switch (handler) {

  case PRE_CALL:
    {
    bool isSizeValid = Properties::isArgNonNegative(memRegionSize);
    if(!isSizeValid){
        BReporter->reportInvalidSizeEntry(C, Call);
        return;
    }

    bool isAlignmentValid = Properties::isArgNonNegative(alignmentVal);
    if(!isAlignmentValid){
        BReporter->reportInvalidSizeEntry(C, Call);
        return;
    }
    }
    break;
  case POST_CALL:
    {
      perormSynchronization(C, State);
    }
    break;
  }
}

void handleMemoryAllocationsWithCalloc(int handler, const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter){

  int countArgIndex = 0, sizeArgIndex = 1;

  ProgramStateRef State = C.getState();
  const SVal numElements = C.getSVal(Call.getArgExpr(countArgIndex));
  const SVal sizeOfElement = C.getSVal(Call.getArgExpr(sizeArgIndex));

  switch (handler) {

  case PRE_CALL:
  {
    bool isSizeValid = Properties::isArgNonNegative(sizeOfElement);
    if(!isSizeValid){
        BReporter->reportInvalidSizeEntry(C, Call);
        return;
    }

    bool isCountValid = Properties::isArgNonNegative(numElements);
    if(!isCountValid){
        BReporter->reportInvalidSizeEntry(C, Call);
        return;
      }
    }
    break;

    case POST_CALL:
    {
    const MemRegion* ptrRegion = Call.getReturnValue().getAsRegion();
    State = Properties::recordThisAllocation(State, ptrRegion, C.generateErrorNode());
    State = Properties::addToArrayList(State, ptrRegion);
    perormSynchronization(C, State);
    }
    break;
  }
}

void handleSingleNonBlockingWrite(int handler, const CallEvent &Call,
                                          CheckerContext &C, const OpenShmemBugReporter* BReporter){

  int memRegionArgIndex = 0,rankArgIndex = 2;
  ProgramStateRef State = C.getState();
  SymbolRef destVariable = Call.getArgSVal(memRegionArgIndex).getAsSymbol();

  const MemRegion* MR = Call.getArgSVal(memRegionArgIndex).getAsRegion();
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
    std::cout << "shmem_p\n";
    // Checks if the Memory Region is global or static
    if(MR->hasGlobalsOrParametersStorage()){
        State = Properties::addToArrayList(State, ER->getSuperRegion());
        Properties::transformState(C, State);
    }

    bool isRegionSymmetric = Properties::isMemRegionSymmetric(State, ER->getSuperRegion());
    if(!isRegionSymmetric){
      BReporter->reportUnSymmetricAccess(C, Call);
    }

  }
    break;
  case POST_CALL:

    // remove the unintialized variables
    State = Properties::removeFromUnitializedList(State, destVariable);
    // mark as unsynchronized
    State = Properties::markAsUnsynchronized(State, destVariable);

    if (!ER){
      std::cout << "Not an element region\n";
    } else {


      int numberOfElements = 1;
      const void* nE = &numberOfElements;

      // Get the array index 
      DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
      SVal numElements = new SVal(nE, clang::ento::nonloc::LocAsInteger);
      SVal nodeIndex = C.getSVal(Call.getArgExpr(rankArgIndex));

      const MemRegion* parentRegion = ER->getSuperRegion();
      
      State = Properties::taintArray(State, parentRegion, Idx, numElements, nodeIndex);
      Properties::transformState(C, State);
    }

    Properties::transformState(C, State);
    break;
  }

}

void handleSingleNonBlockingRead(int handler, const CallEvent &Call,
                                          CheckerContext &C, const OpenShmemBugReporter* BReporter){

  int memRegionArgIndex = 0, rankArgIndex = 2; 
  ProgramStateRef State = C.getState();
  const MemRegion *const MR = Call.getArgSVal(memRegionArgIndex).getAsRegion();
  const ElementRegion *const ER = dyn_cast<ElementRegion>(MR);
  bool isRegionSymmetric = Properties::isMemRegionSymmetric(State, ER->getSuperRegion());
    
  switch (handler) {

  case PRE_CALL:
    std::cout << "shmem_g\n";
    if(!isRegionSymmetric){
      BReporter->reportUnSymmetricAccess(C, Call);
      // return;
    }

    if (!ER){
      std::cout << "Not an element region\n";
    } else {

      int numberOfElements = 1;
      const void* nE = &numberOfElements;

      DefinedOrUnknownSVal Idx = ER->getIndex().castAs<DefinedOrUnknownSVal>();
      SVal num_elements = new SVal(nE, clang::ento::nonloc::LocAsInteger);
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
 * @brief Provide implementation of routine types
 *
 * @return routineHandlers
 */
 routineHandlers addHandlers() {
  routineHandlers handlers;

  handlers.emplace(OpenShmemConstants::SHMEM_MALLOC,
                   std::make_pair(MEMORY_ALLOC, (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_FREE,
                   std::make_pair(MEMORY_DEALLOC, (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_BARRIER,
                   std::make_pair(SYNCHRONIZATION, (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_PUT,
                   std::make_pair(NON_BLOCKING_WRITE, (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_GET,
                   std::make_pair(READ_FROM_MEMORY,  (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_FINALIZE,
                   std::make_pair(FINAL_CALL,  (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_INIT,
                   std::make_pair(INIT_CALL,  (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_REALLOC,
                   std::make_pair(MEMORY_REALLOC, handleMemoryReallocations));
  handlers.emplace(OpenShmemConstants::SHMEM_ALIGN,
                   std::make_pair(MEMORY_ALIGN, handleMemoryAlignments));
  handlers.emplace(OpenShmemConstants::SHMEM_CALLOC,
                   std::make_pair(MEMORY_ALLOC, handleMemoryAllocationsWithCalloc));
  handlers.emplace(OpenShmemConstants::SHMEM_P,
                   std::make_pair(NON_BLOCKING_WRITE, handleSingleNonBlockingWrite));
  handlers.emplace(OpenShmemConstants::SHMEM_G,
                   std::make_pair(READ_FROM_MEMORY, handleSingleNonBlockingRead));

  return handlers;
}
}
}

OpenSHMEMChecker::OpenSHMEMChecker(routineHandlers (*addHandlers)()):PGASChecker((addHandlers)){

}


/**
 * @brief Register the new checker
 *
 * @param mgr
 */
void ento::registerOpenSHMEMChecker(CheckerManager &mgr) {
  mgr.registerChecker<clang::ento::OpenSHMEMChecker, routineHandlers (*)()>(addHandlers);
}

bool ento::shouldRegisterOpenSHMEMChecker(const LangOptions &LO) {
  return true;
}

