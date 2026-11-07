using System.Globalization;

namespace LibertyManager;

public static class OriginalXboxEffects
{
    public const int Values = 22;
    public static byte[] Tuning(string world, string rim, string car)
    {
        var values = new float[24, 4, Values];
        void Read(string text, int offset, params int[] widths)
        {
            var rows = text.Split('\n').Select(l => l.Trim()).Where(l => l.Length > 0 && !l.StartsWith('#')).ToArray();
            if (rows.Length != widths.Length * 24) throw new InvalidDataException("Unsupported Neo tuning table length.");
            for (var block = 0; block < widths.Length; block++)
            {
                var width = widths[block];
                for (var hour = 0; hour < 24; hour++)
                {
                    var fields = rows[block * 24 + hour].Split('\t', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
                    // Retail tables append an hour label after the four weather columns.
                    if (fields.Length < 4) throw new InvalidDataException("Unsupported Neo weather table.");
                    for (var weather = 0; weather < 4; weather++)
                    {
                        var components = fields[weather].Split(',', StringSplitOptions.TrimEntries);
                        if (components.Length != width) throw new InvalidDataException("Invalid Neo tuning value.");
                        for (var component = 0; component < width; component++)
                        {
                            if (!float.TryParse(components[component], NumberStyles.Float, CultureInfo.InvariantCulture, out var value) || !float.IsFinite(value))
                                throw new InvalidDataException("Non-finite Neo tuning value.");
                            if (width == 4) value /= component == 3 && offset >= 14 ? 100f : 255f;
                            values[hour, weather, offset + component] = value;
                        }
                    }
                }
                offset += width;
            }
        }
        Read(world, 0, 1); Read(rim, 1, 4, 4, 1, 1, 1); Read(car, 12, 1, 1, 4, 4);
        using var stream = new MemoryStream(); using var writer = new BinaryWriter(stream);
        writer.Write("NEO1"u8);
        foreach (var value in values) writer.Write(value);
        return stream.ToArray();
    }

    public static async Task Prepare(ManagerSettings settings, ICommandRunner runner, string disc, string stage, string scratch, string converter, CancellationToken token)
    {
        var neo = Path.Combine(stage, "neo"); Directory.CreateDirectory(neo);
        File.WriteAllBytes(Path.Combine(neo, "tuning.bin"), Tuning(File.ReadAllText(Path.Combine(disc, "neo/worldtweakingtable.dat")),
            File.ReadAllText(Path.Combine(disc, "neo/rimtweakingtable.dat")), File.ReadAllText(Path.Combine(disc, "neo/cartweakingtable.dat"))));
        var images = Path.Combine(scratch, "neo-images"); Directory.CreateDirectory(images);
        foreach (var source in OriginalXboxModels.Disc.Files.Keys.Where(p => p.StartsWith("neo/", StringComparison.Ordinal) && p.EndsWith(".dds", StringComparison.Ordinal)))
        {
            var output = Path.Combine(images, Path.GetFileNameWithoutExtension(source) + ".png");
            await runner.Run(settings.Ffmpeg, ["-nostdin", "-v", "error", "-xerror", "-i", Paths.Child(disc, source), "-frames:v", "1", "-n", output], showOutput: false, token: token);
        }
        await runner.Run(converter, ["--neo", images, Path.Combine(neo, "neo.txd")], showOutput: false, token: token);
    }
}
