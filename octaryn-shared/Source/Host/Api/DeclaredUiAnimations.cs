using System.Globalization;
using System.Text.RegularExpressions;

namespace Octaryn.Shared.Host.Api;

internal static class DeclaredUiAnimations
{
    private static readonly Regex Rule=new("@keyframes\\s+([a-zA-Z_][a-zA-Z0-9_.-]{0,127})\\s*\\{((?:[^{}@]*\\{[^{}@]*\\})+)\\s*\\}",
        RegexOptions.IgnoreCase|RegexOptions.CultureInvariant,TimeSpan.FromSeconds(1));
    private static readonly Regex Frame=new("([^{}]+)\\{([^{}]*)\\}",
        RegexOptions.CultureInvariant,TimeSpan.FromSeconds(1));
    private static readonly Regex Property=new("^[a-zA-Z][a-zA-Z0-9-]{0,63}$",
        RegexOptions.CultureInvariant,TimeSpan.FromSeconds(1));

    // Strip only structurally admitted rules for the unknown-at-rule check.
    // The original CSS still passes through the ordinary confined URL resolver.
    internal static string StripValidated(string css)
    {
        var names=new HashSet<string>(StringComparer.Ordinal);
        return Rule.Replace(css,rule=>
        {
            if(names.Count>=64 || !names.Add(rule.Groups[1].Value))
                throw new InvalidOperationException("UI keyframe count or identity admission exceeded.");
            var count=0;
            var remaining=Frame.Replace(rule.Groups[2].Value,frame=>
            {
                if(++count>64)throw new InvalidOperationException("UI animation frame count admission exceeded.");
                var selectors=frame.Groups[1].Value.Trim().Split(',');
                if(selectors.Length>16)throw new InvalidOperationException("UI animation selector count admission exceeded.");
                foreach(var selector in selectors)
                {
                    var value=selector.Trim();
                    if(value is "from" or "to")continue;
                    if(!value.EndsWith('%') || !double.TryParse(value[..^1],NumberStyles.AllowDecimalPoint,
                        CultureInfo.InvariantCulture,out var percent) || !double.IsFinite(percent) || percent<0 || percent>100)
                        throw new InvalidOperationException("Invalid UI animation frame selector.");
                }
                var properties=0;
                foreach(var row in frame.Groups[2].Value.Split(';'))
                {
                    var text=row.Trim();if(text.Length==0)continue;var colon=text.IndexOf(':');
                    if(++properties>64 || colon<1 || !Property.IsMatch(text[..colon].Trim()) ||
                        text[(colon+1)..].Trim().Length is 0 or >4096)
                        throw new InvalidOperationException("Invalid UI animation property.");
                }
                if(properties==0)throw new InvalidOperationException("Empty UI animation frame.");
                return "";
            });
            if(remaining.Trim().Length!=0)throw new InvalidOperationException("Malformed UI animation frames.");
            return "";
        });
    }
}
