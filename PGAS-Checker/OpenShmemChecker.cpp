#include "OpenShmemChecker.h"

using namespace clang;
using namespace ento;

namespace clang{
namespace ento{


void handleMemoryReallocations(int handler, const CallEvent &Call,
                                                CheckerContext &C, const OpenShmemBugReporter* BReporter){

  int memRegionArgIndex = 0, memRegionSizeIndex = 1;
  ProgramStateRef State = C.getState();
  const MemRegion* reAllocVariable = Call.getArgSVal(memRegionArgIndex).getAsRegion()->getBaseRegion();
  const SVal memRegionSize = C.getSVal(Call.getArgExpr(memRegionSizeIndex));

  switch (handler) {

  case PRE_CALL:
    bool isRegionSymmetric = Properties::isMemRegionSymmetric(State, reAllocVariable);
    if(!isRegionSymmetric){
        BReporter->reportUnSymmetricAccess(C, Call);
        return;
    }
    bool isSizeValid = Properties::isMemoryAllocationValid(memRegionSize);
    if(!isSizeValid){
        BReporter->reportInvalidSizeEntry(C, Call);
        return;
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
  handlers.emplace(OpenShmemConstants::SHMEM_GET,
                   std::make_pair(READ_FROM_MEMORY,  (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_FINALIZE,
                   std::make_pair(FINAL_CALL,  (Handler)NULL));
  handlers.emplace(OpenShmemConstants::SHMEM_REALLOC,
                   std::make_pair(MEMORY_REALLOC, handleMemoryReallocations));

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

