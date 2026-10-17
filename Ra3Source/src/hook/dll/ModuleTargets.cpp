#include "Targets.h"

namespace ra3hook {

// Declare append<Module>Targets() from dll/targets/<Module>Targets.cpp here and
// call it below; keys must stay unique across all tables.
void appendPlayerTargets(std::vector<Target>& table);
void appendNetTargets(std::vector<Target>& table);
void appendRenderTargets(std::vector<Target>& table);
void appendGameClientTargets(std::vector<Target>& table);
void appendObjectLogicTargets(std::vector<Target>& table);

void appendModuleTargets(std::vector<Target>& table)
{
    appendPlayerTargets(table);
    appendNetTargets(table);
    appendRenderTargets(table);
    appendGameClientTargets(table);
    appendObjectLogicTargets(table);
}

}  // namespace ra3hook
