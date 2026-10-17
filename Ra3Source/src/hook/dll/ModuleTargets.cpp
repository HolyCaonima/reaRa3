#include "Targets.h"

namespace ra3hook {

// Declare append<Module>Targets() from dll/targets/<Module>Targets.cpp here and
// call it below; keys must stay unique across all tables.
void appendRenderTargets(std::vector<Target>& table);

void appendModuleTargets(std::vector<Target>& table)
{
    appendRenderTargets(table);
}

}  // namespace ra3hook
