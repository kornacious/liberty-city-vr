using System.Globalization;
using System.Text;
using LibertyManager;

internal static class XboxEffectChecks
{
    public static async Task Run(Func<string, Func<Task>, Task> test, Action<bool, string> assert)
    {
        string Row(string name, int id) => name + "\t" + id + "\t" + string.Join("\t", Enumerable.Repeat("0", 36));
        string[] Rows() => OriginalXboxParticles.Names.Select(Row).ToArray();
        void Reject(Action action)
        {
            try { action(); } catch (InvalidDataException) { return; }
            throw new Exception("Malformed effect input accepted.");
        }
        await test("Xbox particle names preserve every PC ID while appending moving car flames", () =>
        {
            var rows = Rows(); var output = OriginalXboxParticles.Normalize(string.Join('\n', rows.Reverse()));
            var normalized = output.Split('\n');
            assert(normalized.Length == 71 && normalized[69] == ";the end", "Complete bounded configuration terminator");
            for (var i = 0; i < rows.Length; i++) assert(normalized[i] == rows[i], "Reordered values stay with their named effect");
            assert(normalized[21].StartsWith("CARFLAME\t") && normalized[22].StartsWith("STEAM\t") && normalized[68].StartsWith("CARFLAME_MOVING\t"), "Existing steam ID is never shifted by Xbox insertion");
            return Task.CompletedTask;
        });
        await test("Xbox particle preparation rejects missing, duplicate, unknown and non-finite definitions", () =>
        {
            var rows = Rows();
            Reject(() => OriginalXboxParticles.Normalize(string.Join('\n', rows.Skip(1))));
            Reject(() => OriginalXboxParticles.Normalize(string.Join('\n', rows.Append(rows[0]))));
            rows[0] = rows[0].Replace("SPARK", "UNKNOWN"); Reject(() => OriginalXboxParticles.Normalize(string.Join('\n', rows)));
            rows = Rows(); rows[0] = rows[0].Replace("\t0", "\tNaN"); Reject(() => OriginalXboxParticles.Normalize(string.Join('\n', rows)));
            return Task.CompletedTask;
        });
        string Table(int width, int blocks) => string.Join('\n', Enumerable.Range(0, blocks).SelectMany(_ => Enumerable.Range(0, 24)
            .Select(hour => string.Join('\t', Enumerable.Range(0, 4).Select(weather => width == 1
                ? (hour * 4 + weather).ToString(CultureInfo.InvariantCulture) : "255,128,64,100")) + "\thour label")));
        await test("Neo tuning retains clock/weather ordering and normalizes colour and light-alpha units", () =>
        {
            var scalar = Table(1, 1); var colors = Table(4, 1);
            var bytes = OriginalXboxEffects.Tuning(scalar, string.Join('\n', colors, colors, scalar, scalar, scalar), string.Join('\n', scalar, scalar, colors, colors));
            assert(bytes.Length == 4 + 24 * 4 * 22 * 4 && Encoding.ASCII.GetString(bytes, 0, 4) == "NEO1", "Versioned bounded table");
            float Value(int hour, int weather, int field) => BitConverter.ToSingle(bytes, 4 + ((hour * 4 + weather) * 22 + field) * 4);
            assert(Value(12, 2, 0) == 50 && Value(23, 3, 13) == 95, "Time/weather and specular exponent columns retained");
            assert(Value(0, 0, 1) == 1 && Math.Abs(Value(0, 0, 2) - 128f / 255) < 1e-6 && Value(0, 0, 17) == 1, "RGBA colour and percent light intensity use their authored units");
            Reject(() => OriginalXboxEffects.Tuning("0", colors, scalar));
            Reject(() => OriginalXboxEffects.Tuning(scalar.Replace("0\t", "NaN\t"), string.Join('\n', colors, colors, scalar, scalar, scalar), string.Join('\n', scalar, scalar, colors, colors)));
            return Task.CompletedTask;
        });
    }
}
