#ifndef __OPENSHMEM_CHECK
#define __OPENSHMEM_CHECK

#include "PGASChecker.h"

using namespace clang;
using namespace ento;

namespace clang{
namespace ento{

routineHandlers addHandlers();

class OpenSHMEMChecker : public PGASChecker{
	public:
		OpenSHMEMChecker(routineHandlers (*addHandlers)());
};

namespace OpenShmemConstants {
const std::string SHMEM_MALLOC = "shmem_malloc";
const std::string SHMEM_GET = "shmem_get";
const std::string SHMEM_PUT = "shmem_put";
const std::string SHMEM_FREE = "shmem_free";
const std::string SHMEM_BARRIER = "shmem_barrier_all";
const std::string SHMEM_FINALIZE = "shmem_finalize";
const std::string SHMEM_CALLOC = "shmem_calloc";
const std::string SHMEM_REALLOC = "shmem_realloc";
const std::string SHMEM_ALIGN = "shmem_align";
const std::string SHMEM_INIT = "shmem_init";
const std::string SHMEM_P = "shmem_p";
const std::string SHMEM_G = "shmem_g";
} // namespace OpenShmemConstants
}
}

#endif