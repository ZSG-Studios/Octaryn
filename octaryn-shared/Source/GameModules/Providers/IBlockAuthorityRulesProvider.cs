using Octaryn.Shared.World;

namespace Octaryn.Shared.GameModules;

public interface IBlockAuthorityRulesProvider
{
    IBlockAuthorityRules BlockAuthorityRules { get; }
}
