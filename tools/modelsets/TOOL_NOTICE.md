# Original Xbox asset converter notice

The Manager prepares a complete local game variant from the user's own original Xbox
GTA III US 1.0 ISO. It never downloads game data. The XDVDFS reader is implemented
in C#; extract-xiso and Python are not required.

`xbox-convert.cpp` links the bundled MIT-licensed librw fork and lzokay sources.
`vendor/lzokay` is by Jack Andersen (2018), obtained from
https://github.com/jackoalan/lzokay; its MIT license is retained beside the source.
It implements LZO decompression; no game data is included in these sources.
The CMake target runs headlessly and requires a C++17 compiler.

`xbox-disc.json` contains only file sizes and hashes identifying the supported
retail asset baseline, not original files. Source images, extracted files and
converted variants remain local and must never be redistributed or committed.

The obsolete HD-pack builder, `xbox-pack.json`, texture compressor source/binary
and their regression adapters have been removed. The Original Xbox importer does
not recompress or cap retail textures. CMake/CTest and the host compiler are
external build tools; see the [tools inventory](../README.md) and
[root notices](../../NOTICE.md).
