using Octaryn.Shared.World;

namespace Octaryn.Shared.GameModules;

public interface IWorldGenerationRulesProvider
{
    IWorldGenerationRules WorldGenerationRules { get; }
}
