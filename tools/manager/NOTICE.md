# Liberty City VR Manager third-party notices

The manager targets .NET 10 and Terminal.Gui 2.5.0. This table is audited against
`packages.lock.json` and the restored packages' `.nuspec` license metadata.
`packages.publish.lock.json` additionally locks Microsoft.NET.ILLink.Tasks
10.0.13 (MIT), used by single-file publishing. Packages are obtained via NuGet,
not vendored as source. Preserve each package's copyright/license text when
redistributing compiled output; the identifiers below are an inventory, not
replacement license texts.

| NuGet package | Locked version | Declared license | Source |
| --- | --- | --- | --- |
| Terminal.Gui | 2.5.0 | MIT | [Upstream](https://github.com/tui-cs/Terminal.Gui) |
| ColorHelper | 1.8.1 | MIT | [Upstream](https://github.com/iamartyom/ColorHelper) |
| JetBrains.Annotations | 2026.2.0 | MIT | [Upstream](https://www.jetbrains.com/help/resharper/Code_Analysis__Code_Annotations.html) |
| Markdig | 1.3.2 | BSD-2-Clause | [Upstream](https://xoofx.github.io/markdig) |
| Microsoft.Extensions.Configuration | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.Configuration.Abstractions | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.Configuration.Binder | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.Configuration.FileExtensions | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.Configuration.Json | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.DependencyInjection.Abstractions | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.FileProviders.Abstractions | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.FileProviders.Physical | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.FileSystemGlobbing | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.Logging.Abstractions | 10.0.7 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.Options | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Microsoft.Extensions.Primitives | 10.0.11 | MIT | [Upstream](https://dot.net/) |
| Onigwrap | 1.0.11 | MIT | [Upstream](https://github.com/aikawayataro/Onigwrap) |
| System.IO.Abstractions | 22.2.0 | MIT | [Upstream](https://github.com/TestableIO/System.IO.Abstractions) |
| TestableIO.System.IO.Abstractions | 22.2.0 | MIT | [Upstream](https://github.com/TestableIO/System.IO.Abstractions) |
| TestableIO.System.IO.Abstractions.Wrappers | 22.2.0 | MIT | [Upstream](https://github.com/TestableIO/System.IO.Abstractions) |
| Testably.Abstractions.FileSystem.Interface | 10.3.0 | MIT | [Upstream](https://github.com/Testably/Testably.Abstractions) |
| TextMateSharp | 2.0.4 | MIT | [Upstream](https://github.com/danipen/TextMateSharp) |
| TextMateSharp.Grammars | 2.0.4 | MIT | [Upstream](https://github.com/danipen/TextMateSharp) |
| Wcwidth | 4.0.1 | MIT | [Upstream](https://github.com/spectreconsole/wcwidth) |

Onigwrap also ships native Oniguruma (BSD-2-Clause, K. Kosako) and upstream
fluentCODE/onigwrap and TextMateSharp notices. Its package notice is
retained in [ONIGWRAP-NOTICES.txt](ONIGWRAP-NOTICES.txt). TextMateSharp.Grammars
contains third-party language grammars/themes; retain the notices in the restored
package and upstream grammar sources when redistributing those assets.

Self-contained publishing also carries .NET runtime code and native components.
Use the actual runtime pack's LICENSE and THIRD-PARTY-NOTICES files for the target
RID, together with the NuGet package licenses, when preparing binary releases.
The supported public project release is a source kit; local publish output must
remain outside Git. [Root notices](../../NOTICE.md) cover external build tools.

The project file copies this inventory and the native Onigwrap notice alongside
build/publish output so single-file packaging does not hide those notices.
