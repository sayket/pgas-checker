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

    bool isReallocEligible = Properties::isMemRegionAvailable(State, reAllocVariable);
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

//#define NB_WRITE ()

  handlers.emplace("shmem_int64_put_nbi",
                   std::make_pair(NON_BLOCKING_WRITE, (Handler)NULL));
  handlers.emplace("shmem_int64_put",
                   std::make_pair(NON_BLOCKING_WRITE, (Handler)NULL));
  handlers.emplace("shmem_int_put",
                   std::make_pair(NON_BLOCKING_WRITE, (Handler)NULL));


  handlers.emplace(OpenShmemConstants::SHMEM_GET,
                   std::make_pair(READ_FROM_MEMORY,  (Handler)NULL));
  handlers.emplace("shmem_getmem_nbi",
                   std::make_pair(READ_FROM_MEMORY, (Handler)NULL));
  handlers.emplace("shmem_getmem",
                   std::make_pair(READ_FROM_MEMORY, (Handler)NULL));
  handlers.emplace("shmem_int_get",
                   std::make_pair(READ_FROM_MEMORY, (Handler)NULL));


  handlers.emplace(OpenShmemConstants::SHMEM_FINALIZE,
                   std::make_pair(FINAL_CALL,  (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_REALLOC,
                   std::make_pair(MEMORY_REALLOC, handleMemoryReallocations));
  handlers.emplace(OpenShmemConstants::SHMEM_ALIGN,
                   std::make_pair(MEMORY_ALIGN, handleMemoryAlignments));
  handlers.emplace(OpenShmemConstants::SHMEM_CALLOC,
                   std::make_pair(MEMORY_ALLOC, handleMemoryAllocationsWithCalloc));

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

