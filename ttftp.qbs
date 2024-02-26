import qbs
Project
{
    name: "TTFTP"
CppApplication {
    consoleApplication: true
    install: true
    Group
    {
        name: "Sourcefiles"
        prefix: "src/"
    files: [
            "Tftphelpsefs.h",
            "main.cpp",
            "tftpclient.cpp",
            "tftpclient.h",
            "tftpreceiver.h",
            "tftpsender.h",
            "tftpserver.cpp",
            "tftpserver.h",
        ]
    }
    cpp.dynamicLibraries: ["SDL2", "SDL2_mixer"]


}

SubProject {
        filePath: "ttftpTest/ttftpTest.qbs"
}
}
