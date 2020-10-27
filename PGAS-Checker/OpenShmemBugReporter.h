#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"

using namespace clang;
using namespace ento;

class OpenShmemBugReporter {
public: 
	OpenShmemBugReporter(const CheckerBase &CB){
    	UnsafeArrayReadBug = new BugType(&CB, "Unsafe Read", "Symmetric Array"); 
  	}
  	void reportUnsafeRead(const clang::ento::MemRegion *const RequestRegion, const ExplodedNode *const ExplNode, BugReporter &BReporter);

private:
	BugType UnsafeArrayReadBug;
};