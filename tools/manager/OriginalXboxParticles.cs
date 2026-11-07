using System.Globalization;

namespace LibertyManager;

public static class OriginalXboxParticles
{
    // Stable re3 enum order, with the Xbox-only type appended. These are engine
    // identifiers, not retail configuration values.
    public static readonly string[] Names = [
        "SPARK", "SPARK_SMALL", "WHEEL_DIRT", "WHEEL_WATER", "BLOOD", "BLOOD_SMALL", "BLOOD_SPURT", "DEBRIS", "DEBRIS2", "WATER", "FLAME", "FIREBALL", "GUNFLASH", "GUNFLASH_NOANIM", "GUNSMOKE", "GUNSMOKE2", "SMOKE", "SMOKE_SLOWMOTION", "GARAGEPAINT_SPRAY", "SHARD", "SPLASH", "CARFLAME", "STEAM", "STEAM2", "STEAM_NY", "STEAM_NY_SLOWMOTION", "ENGINE_STEAM", "RAINDROP", "RAINDROP_SMALL", "RAIN_SPLASH", "RAIN_SPLASH_BIGGROW", "RAIN_SPLASHUP", "WATERSPRAY", "EXPLOSION_MEDIUM", "EXPLOSION_LARGE", "EXPLOSION_MFAST", "EXPLOSION_LFAST", "CAR_SPLASH", "BOAT_SPLASH", "BOAT_THRUSTJET", "BOAT_WAKE", "WATER_HYDRANT", "WATER_CANNON", "EXTINGUISH_STEAM", "PED_SPLASH", "PEDFOOT_DUST", "HELI_DUST", "HELI_ATTACK", "ENGINE_SMOKE", "ENGINE_SMOKE2", "CARFLAME_SMOKE", "FIREBALL_SMOKE", "PAINT_SMOKE", "TREE_LEAVES", "CARCOLLISION_DUST", "CAR_DEBRIS", "HELI_DEBRIS", "EXHAUST_FUMES", "RUBBER_SMOKE", "BURNINGRUBBER_SMOKE", "BULLETHIT_SMOKE", "GUNSHELL_FIRST", "GUNSHELL", "GUNSHELL_BUMP1", "GUNSHELL_BUMP2", "TEST", "BIRD_FRONT", "RAINDROP_2D", "CARFLAME_MOVING"];

    public static string Normalize(string source)
    {
        var rows = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var raw in source.Split('\n'))
        {
            var line = raw.Trim();
            if (line.Length == 0 || line.StartsWith(';')) continue;
            var fields = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (fields.Length != 38 || !Names.Contains(fields[0], StringComparer.Ordinal) || !rows.TryAdd(fields[0], string.Join("\t", fields)))
                throw new InvalidDataException("Unsupported Xbox particle definition.");
            foreach (var value in fields.Skip(1))
                if (!double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var number) || !double.IsFinite(number))
                    throw new InvalidDataException("Invalid Xbox particle setting.");
        }
        if (rows.Count != Names.Length) throw new InvalidDataException("Incomplete Xbox particle collection.");
        return string.Join("\n", Names.Select(name => rows[name])) + "\n;the end\n";
    }
}
