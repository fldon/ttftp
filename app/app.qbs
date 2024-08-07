CppApplication {
    name: "TTFTP"
    consoleApplication: true
    install: true
    Group
    {
        name: "Sourcefiles"
        prefix: "src/"
    files: [
            "main.cpp",
            "tftpclient.cpp",
            "tftpclient.h",
            "tftphelpdefs.h",
            "tftphelpdefs.cpp",
            "tftpreceiver.h",
            "tftpreceiver.cpp",
            "tftpsender.cpp",
            "tftpsender.h",
            "tftpserver.cpp",
            "tftpserver.h",
            "tftpmessages.cpp",
            "tftpmessages.h"
        ]
    }
    //cpp.dynamicLibraries: ["SDL2", "SDL2_mixer"]
}
