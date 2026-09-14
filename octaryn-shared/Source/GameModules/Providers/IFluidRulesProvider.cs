using Octaryn.Shared.World;

namespace Octaryn.Shared.GameModules;

public interface IFluidRulesProvider
{
    FluidRules FluidRules { get; }
}
