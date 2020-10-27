#ifndef __PGAS_CHECK
#define __PGAS_CHECK

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
// #include "OpenShmemBugReporter.h"
#include <clang/StaticAnalyzer/Frontend/CheckerRegistry.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <bits/stdc++.h>

#define BITSET_SIZE 25

using namespace clang;
using namespace ento;

// event handlers for specific routines
typedef void (*Handler)(int handler, const CallEvent &Call, CheckerContext &C);

// pgas specific routine types
typedef enum routines {
  MEMORY_ALLOC,
  MEMORY_DEALLOC,
  SYNCHRONIZATION,
  BLOCKING_WRITE,
  NON_BLOCKING_WRITE,
  READ_FROM_MEMORY
} Routine;

// ( non_blocking_routine_type, event_handler  )
typedef std::pair<Routine, Handler> Pair;

typedef std::bitset<BITSET_SIZE> Tracker; 

// hold the mapping between routine name of a specific pgas programming model
// routine, routine type and the event handler
// [shmem_put] => Pair
typedef std::unordered_map<std::string, Pair> routineHandlers;

// this represents the possible states of a symmetric variable;
// for instance the possible states here are Synchronized, Unsynchronized
// this is currently used as a value for the program state map
// program state map is internally an llvm::immutable_map which is used to track
// the variables through the process of static analyis; so
// there are three main data structures for the purpose;
// Map, Set and List;
// Please look into Custom Program States in
// https://clang-analyzer.llvm.org/checker_dev_manual.html
struct RefState {
private:
  enum Kind { Synchronized, Unsynchronized } K;
  RefState(Kind InK) : K(InK) {}

public:
  bool isSynchronized() const { return K == Synchronized; }
  bool isUnSynchronized() const { return K == Unsynchronized; }

  static RefState getSynchronized() { return RefState(Synchronized); }
  static RefState getUnsynchronized() { return RefState(Unsynchronized); }

  // overloading of == comparison operator
  bool operator==(const RefState &X) const { return K == X.K; }

  void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(K); }
};

// map to hold the state of the variable; synchronized or unsynchronized
struct CheckerState {};
// LLVM Immutable Map to track the state of a symmetric variable
// to have a string(or any type other than a symbol ref) as the map key
// llvm::ImmutableMap<std::string, RefState>
typedef llvm::ImmutableMap<SymbolRef, RefState> PGASMapImpl;
// LLVM Immmutable Set to track uninitialized and freed variables allocated in
// the program(via *_malloc type routines)
typedef llvm::ImmutableSet<SymbolRef> PGASSetImpl;


typedef llvm::ImmutableSet<MemRegion*> PGASMemRegionsImpl;
// typedef llvm::ImmutableMap<MemRegion*, Tracker> PGASMemRegionMap;

// the following lines enables the developer to declare a custom immutable map
// with custom keys and value; for instance declaring a map using the
// REGISTER_MAP_WITH_PROGRAMSTATE macro allows you to only have SymbolRef as the
// key, for instance if you find a need to use a key of a different type this
// Please refer to this
// https://github.com/llvm-mirror/clang/blob/master/lib/StaticAnalyzer/Checkers/MPI-Checker/MPITypes.h
namespace clang{
namespace ento{

template <>
struct ProgramStateTrait<CheckerState>
    : public ProgramStatePartialTrait<PGASMapImpl> {
  static void *GDMIndex() {
    static int index = 0;
    return &index;
  }
};

class TrackingClass {

private:

public:
  std::vector<std::pair<DefinedOrUnknownSVal, DefinedOrUnknownSVal>> trackingVector;
  Tracker tracker;

  void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(tracker.to_ulong()); }
  
  void updateTracker(DefinedOrUnknownSVal startIndex, DefinedOrUnknownSVal numElements) {

    std::pair<DefinedOrUnknownSVal, DefinedOrUnknownSVal> p1 = std::make_pair(startIndex, numElements);
    
    if(startIndex.isUnknownOrUndef()){
      std::cout << "Start Index has an unknown or undefined SVal \n";
    }

    if(numElements.isUnknownOrUndef()){
      std::cout << "Number of elements has an unknown or undefined SVal \n";
    }

    p1.first = startIndex;
    p1.second = numElements;

    trackingVector.push_back(p1);
  }

  bool isRangeEmpty(DefinedOrUnknownSVal startIndex, DefinedOrUnknownSVal numElements, CheckerContext &C) const{

    std::cout << "All Fine till here 1\n";
    ProgramStateRef state = C.getState();
    SValBuilder &svalBuilder = C.getSValBuilder();
    std::cout << "All Fine till here 2\n";
    // if(svalBuilder != null){
    //   std::cout << "Not null\n";
    // }
    if(startIndex.isUnknownOrUndef()){
      std::cout << "Start Index has an unknown or undefined SVal \n";
    }

    if(numElements.isUnknownOrUndef()){
      std::cout << "Number of elements has an unknown or undefined SVal \n";
    }

    DefinedOrUnknownSVal endIndex = svalBuilder.evalBinOp(state, BO_Add, startIndex, numElements, svalBuilder.getArrayIndexType()).castAs<DefinedOrUnknownSVal>();
    endIndex = svalBuilder.evalBinOp(state, BO_Sub, endIndex, svalBuilder.makeArrayIndex(1), svalBuilder.getArrayIndexType()).castAs<DefinedOrUnknownSVal>();
    auto it = trackingVector.begin();
    std::cout << "All Fine till here 3\n";
    for(; it != trackingVector.end(); ++it){
          std::cout << "All Fine till here 4\n";
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

  bool operator==(const TrackingClass &X) const { return trackingVector == X.trackingVector; }
};

// Declaration of the Base Checker
class PGASChecker : public Checker<check::PostCall, check::PreCall, check::Location> {

private:
  void eventHandler(int handler, std::string &routineName,
                    const CallEvent &Call, CheckerContext &C) const;
  void addDefaultHandlers();
  Handler getDefaultHandler(Routine routineType) const;

public:
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
  void checkLocation(SVal Loc, bool IsLoad, const Stmt *S,
                      CheckerContext &C) const;

  // OpenShmemBugReporter BReporter;
  PGASChecker(routineHandlers (*addHandlers)());
};

}
}
// set of unitilized variables;
// llvm immutable set of type PGASSetImpl
REGISTER_TRAIT_WITH_PROGRAMSTATE(UnintializedVariables, PGASSetImpl) //Like a path-sensitive map and very performant
// // set of freed variables
REGISTER_TRAIT_WITH_PROGRAMSTATE(FreedVariables, PGASSetImpl)
// // set of array regions
REGISTER_TRAIT_WITH_PROGRAMSTATE(ArrayRegions, PGASMemRegionsImpl)
//map of tracked indices
REGISTER_MAP_WITH_PROGRAMSTATE(RegionTracker, const MemRegion*, TrackingClass) 

enum HANDLERS { PRE_CALL = 0, POST_CALL = 1 };

// place all error messages here
// would be better to move it to a different file
namespace ErrorMessages {
const std::string VARIABLE_NOT_SYMMETRIC = "Not a symmetric variable";
const std::string UNSYNCHRONIZED_ACCESS = "Unsynchronized access to variable";
const std::string ACCESS_FREED_VARIABLE = "Trying to access a freed variable";
const std::string ACCESS_UNINTIALIZED_VARIABLE =
    "Trying to access a unitialized variable";
const std::string ACCESS_ARRAY_VARIABLE =
    "Some Memory Region Changed";    
} // namespace ErrorMessages

// function declarations for default handlers
// all handlers take in handler type, CallEvent, CheckerContext
// Call Events contain all the neccessary information pertaining to the function
// call such as arguments, return types etc.
// https://clang.llvm.org/doxygen/classclang_1_1ento_1_1CallEvent.html
// As the name suggests the CheckerContext provides the context of the checker
// such as retrieving the current program state, transform the program state
// graph
// https://clang.llvm.org/doxygen/classclang_1_1ento_1_1CheckerContext.html
namespace DefaultHandlers {
void handleMemoryAllocations(int handler, const CallEvent &Call,
                             CheckerContext &C);
void handleBarriers(int handler, const CallEvent &Call, CheckerContext &C);
void handleNonBlockingWrites(int hanndler, const CallEvent &Call,
                             CheckerContext &C);
void handleBlockingWrites(int handler, const CallEvent &Call,
                          CheckerContext &C);
void handleReads(int handler, const CallEvent &Call, CheckerContext &C);
void handleMemoryDeallocations(int handler, const CallEvent &Call,
                               CheckerContext &C);
} // namespace DefaultHandlers

// these are the common properties which shared across different PGAS
// programming models; can think of it as utilities
// Implementation of the Property Layer
namespace Properties {
void transformState(CheckerContext &C, ProgramStateRef State);
ProgramStateRef removeFromUnitializedList(ProgramStateRef State,
                                          SymbolRef variable);
ProgramStateRef removeFromFreeList(ProgramStateRef State, SymbolRef variable);
ProgramStateRef addToFreeList(ProgramStateRef State, SymbolRef variable);
ProgramStateRef addToUnintializedList(ProgramStateRef State,
                                      SymbolRef variable);
ProgramStateRef removeFromState(ProgramStateRef State, SymbolRef variable);
ProgramStateRef markAsUnsynchronized(ProgramStateRef State, SymbolRef variable);
ProgramStateRef markAsSynchronized(ProgramStateRef State, SymbolRef variable);
ProgramStateRef addToArrayList(ProgramStateRef State, const MemRegion* arrayRegion);
ProgramStateRef taintArray(ProgramStateRef State, const MemRegion* arrayRegion, SVal startIndex, SVal numElements);
bool checkTrackerRange(CheckerContext &C, const MemRegion* arrayRegion, SVal startIndex, SVal numElements);
// void printTheMap(ProgramStateRef State);
bool regionExistsInMap(ProgramStateRef State, const MemRegion* arrayRegion);
} // namespace Properties



#endif