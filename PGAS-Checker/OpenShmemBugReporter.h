#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include <clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h>
#include <iostream>

using namespace clang;
using namespace ento;

class OpenShmemBugReporter {
public: 
	OpenShmemBugReporter(CheckerBase &CB){
		if(!ReadBug) ReadBug.reset(new BuiltinBug(&CB, "Unsafe Read", "Attempt to read from unsynchronized regions of the array"));
		if(!NoFreeBug) NoFreeBug.reset(new BuiltinBug(&CB, "Missing Free", "An Allocated Memory Region has not been freed"));
		if(!DoubleFreeBug) DoubleFreeBug.reset(new BuiltinBug(&CB, "Double Free", "The Allocated Memory Region has been freed twice"));
		if(!UselessBarrierBug) UselessBarrierBug.reset(new BuiltinBug(&CB, "Useless Barrier", "The Allocated Barrier is not required"));
		if(!InvalidSizeBug) InvalidSizeBug.reset(new BuiltinBug(&CB, "Invalid Size", "The size specified for the memory region must be a non-negative integer"));
		if(!InvalidReallocBug) InvalidReallocBug.reset(new BuiltinBug(&CB, "Invalid Reallocation", "The region specified for re-allocation must be allocated first"));
		if(!WronglyPlacedCallBug) WronglyPlacedCallBug.reset(new BuiltinBug(&CB, "Incorrectly Placed Call", "A shmem call can only be used within a shmem_init and shmem_finalize"));
		if(!NonSymmetricAccessBug) NonSymmetricAccessBug.reset(new BuiltinBug(&CB, "Unsymmetric Region Access", "The region that being accessed is not a symmetric variable"));
  	}
  	
  	void reportUnsafeRead(CheckerContext &C,const CallEvent &Call) const {
		ExplodedNode *errorNode = C.generateErrorNode();
      	if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*ReadBug, ReadBug->getDescription(), errorNode);
      	R->addRange(Call.getSourceRange());
      	C.emitReport(std::move(R));
	}

	void reportDoubleFree(CheckerContext &C,const CallEvent &Call) const {
		ExplodedNode *errorNode = C.generateErrorNode();
      	if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*DoubleFreeBug, DoubleFreeBug->getDescription(), errorNode);
      	R->addRange(Call.getSourceRange());
      	C.emitReport(std::move(R));
	}

	void reportNoFree(CheckerContext &C, ExplodedNode* errorNode) const {
		if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*NoFreeBug, NoFreeBug->getDescription(), errorNode);
      	C.emitReport(std::move(R));
	}

	void reportUnSymmetricAccess(CheckerContext &C,const CallEvent &Call) const {
		ExplodedNode *errorNode = C.generateErrorNode();
      	if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*NonSymmetricAccessBug, NonSymmetricAccessBug->getDescription(), errorNode);
      	R->addRange(Call.getSourceRange());
      	C.emitReport(std::move(R));
	}

	void reportInvalidReallocation(CheckerContext &C,const CallEvent &Call) const{
		ExplodedNode *errorNode = C.generateErrorNode();
      	if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*InvalidReallocBug, InvalidReallocBug->getDescription(), errorNode);
      	R->addRange(Call.getSourceRange());
      	C.emitReport(std::move(R));
	}

	void reportInvalidSizeEntry(CheckerContext &C,const CallEvent &Call) const {
		ExplodedNode *errorNode = C.generateErrorNode();
      	if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*InvalidSizeBug, InvalidSizeBug->getDescription(), errorNode);
      	R->addRange(Call.getSourceRange());
      	C.emitReport(std::move(R));
	}

	void reportWronglyPlacedShmemCall(CheckerContext &C,const CallEvent &Call) const {
		ExplodedNode *errorNode = C.generateErrorNode();
      	if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*WronglyPlacedCallBug, WronglyPlacedCallBug->getDescription(), errorNode);
      	R->addRange(Call.getSourceRange());
      	C.emitReport(std::move(R));
	}

private:
	std::unique_ptr<BuiltinBug> ReadBug;
	std::unique_ptr<BuiltinBug> NoFreeBug;
	std::unique_ptr<BuiltinBug> DoubleFreeBug;
	std::unique_ptr<BuiltinBug> InvalidSizeBug;
	std::unique_ptr<BuiltinBug> UselessBarrierBug;
	std::unique_ptr<BuiltinBug> InvalidReallocBug;
	std::unique_ptr<BuiltinBug> WronglyPlacedCallBug;
	std::unique_ptr<BuiltinBug> NonSymmetricAccessBug;
};