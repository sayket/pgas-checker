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
  	}
  	void reportUnsafeRead(CheckerContext &C,const CallEvent &Call) const {
		ExplodedNode *errorNode = C.generateErrorNode();
      	if (!errorNode) return;
      	auto R = std::make_unique<PathSensitiveBugReport>(*ReadBug, ReadBug->getDescription(), errorNode);
      	R->addRange(Call.getSourceRange());
      	C.emitReport(std::move(R));
	}

private:
	std::unique_ptr<BuiltinBug> ReadBug;

};