# ttftp
simple trivial file transfer protocol server and client combo.
For usage info, simply run the binary without parameters.

Implements the following RFCs:

1350 (currently without netascii and mail modes)

2347

2348


TODO:

RFC 2349

Build instructions:

Build using the top level ttftp.qbs project file.

If unfamiliar with qbs, set up a sensible default toolchain profile and then execute

simple_build.sh

in the repo folder. The built binary will be in the bin folder.
