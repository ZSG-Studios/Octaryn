using Octaryn.Shared.Host.Api;

namespace Octaryn.Basegame.Gameplay.Actions;

// Routes module-visible UI actions to registered system handlers. One poller
// owns the queue so systems never compete for it.
public sealed class ActionRouter
{
    private readonly IHostUiApi? _ui;
    private readonly List<(string Prefix, Func<string, bool> Handler)> _handlers = [];

    public ActionRouter(IHostUiApi? ui)
    {
        _ui = ui;
    }

    public void Register(string prefix, Func<string, bool> handler)
    {
        _handlers.Add((prefix, handler));
    }

    public void Tick()
    {
        if (_ui is null)
        {
            return;
        }

        while (_ui.TryPollAction(out var actionId))
        {
            foreach (var (prefix, handler) in _handlers)
            {
                if (actionId.StartsWith(prefix, StringComparison.Ordinal) && handler(actionId))
                {
                    break;
                }
            }
        }
    }
}
