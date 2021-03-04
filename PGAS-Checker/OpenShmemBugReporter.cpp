#include "OpenShmemBugReporter.h"

// using namespace clang;
// using namespace ento;

// void OpenShmemBugReporter::reportUnsafeRead(const MemRegion* ArrayRegion,
//     const ExplodedNode* ExplNode,
//     BugReporter &BReporter){

// 	std::string ErrorText{"Attempt to read from unsynchronized regions of the array " + ArrayRegion->getDescriptiveName()};
// 	auto Report = std::make_unique<PathSensitiveBugReport>(*UnsafeArrayReadBug,
//                                                          ErrorText, ExplNode);

// 	BReporter.emitReport(std::move(Report));
// }