#include "Targets.h"

namespace ra3hook {

// Declare append<Module>Targets() from dll/targets/<Module>Targets.cpp here and
// call it below; keys must stay unique across all tables.
void appendPlayerTargets(std::vector<Target>& table);
void appendNetTargets(std::vector<Target>& table);
void appendRenderTargets(std::vector<Target>& table);
void appendRender8eTargets(std::vector<Target>& table);
void appendGameClientTargets(std::vector<Target>& table);
void appendStringBase2Targets(std::vector<Target>& table);
void appendSkirmishTargets(std::vector<Target>& table);
void appendAudioTargets(std::vector<Target>& table);
void appendBehaviorTargets(std::vector<Target>& table);
void appendObjectLogicTargets(std::vector<Target>& table);
void appendGameClient92Targets(std::vector<Target>& table);

void appendModuleTargets(std::vector<Target>& table)
{
    appendPlayerTargets(table);
    appendNetTargets(table);
    appendRenderTargets(table);
    appendRender8eTargets(table);
    appendGameClientTargets(table);
    appendStringBase2Targets(table);
    appendSkirmishTargets(table);
    appendAudioTargets(table);
    appendBehaviorTargets(table);
    appendObjectLogicTargets(table);
    appendGameClient92Targets(table);
}

}  // namespace ra3hook
