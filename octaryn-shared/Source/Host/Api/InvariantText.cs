using System.Globalization;

namespace Octaryn.Shared.Host.Api;

// Pure, bounded numeric text operations. Does not expose culture objects or process culture state.
public static class InvariantText
{
    public const int MaximumNumericCharacters = 256;

    public static bool TryParseUnsignedInt32(string? text, out int value)
    {
        value = 0;
        return InBudget(text) && int.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out value);
    }

    public static bool TryParseUInt16(string? text, out ushort value)
    {
        value = 0;
        return InBudget(text) && ushort.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out value);
    }

    public static bool TryParseFiniteDouble(string? text, out double value)
    {
        value = 0;
        if (!InBudget(text) || !double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed) || !double.IsFinite(parsed)) return false;
        value = parsed; return true;
    }

    public static string Format(int value) => value.ToString(CultureInfo.InvariantCulture);
    public static string Format(ushort value) => value.ToString(CultureInfo.InvariantCulture);
    public static string Format(double value)
    {
        if (!double.IsFinite(value)) throw new InvalidOperationException("Numeric text requires a finite value.");
        return value.ToString("R", CultureInfo.InvariantCulture);
    }

    private static bool InBudget(string? text) => text is { Length: > 0 and <= MaximumNumericCharacters };
}
