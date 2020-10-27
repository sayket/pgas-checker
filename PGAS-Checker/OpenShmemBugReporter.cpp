#include "OpenShmemBugReporter.h"

using namespace clang;
using namespace ento;

 OpenShmemBugReporter::OpenShmemBugReporter(const CheckerBase &CB){
    	UnsafeArrayReadBug = new BugType(&CB, "Unsafe Read", "Symmetric Array"); 
  	}

void OpenShmemBugReporter::reportUnsafeRead(const clang::ento::MemRegion *const ArrayRegion,
    const ExplodedNode *const ExplNode,
    BugReporter &BReporter){

	std::string ErrorText{"Attempt to read from unsynchronized regions of the array " + ArrayRegion->getDescriptiveName()};
	auto Report = std::make_unique<PathSensitiveBugReport>(*UnsafeArrayReadBug,
                                                         ErrorText, ExplNode);

	BReporter.emitReport(std::move(Report));
}