import qbs
import qbs.Environment
import qbs.FileInfo
import "googlecommon.js" as googleCommon

CppApplication {
    consoleApplication: true

    property string googletestDir: {
        if (typeof Environment.getEnv("GOOGLETEST_DIR") === 'undefined') {
            if ("" === "" && googleCommon.getGTestDir(qbs, undefined) !== "") {
                console.warn("Using googletest from system")
            } else {
                console.warn("Using googletest src dir specified at Qt Creator wizard")
                console.log("set GOOGLETEST_DIR as environment variable or Qbs property to get rid of this message")
            }
            return ""
        } else {
            return Environment.getEnv("GOOGLETEST_DIR")
        }
    }

    cpp.cxxLanguageVersion: "c++14"
    cpp.dynamicLibraries: {
        var tmp = ["gtest", "gmock"];
        if (qbs.hostOS.contains("windows")) {
            return tmp;
        } else {
            return tmp.concat([ "pthread" ]);
        }
    }
    property string projectSourcePath: FileInfo.joinPaths(project.sourceDirectory, "/app/src/")
    cpp.includePaths: [projectSourcePath].concat(googleCommon.getChildPath(qbs, googletestDir, "include"));
    cpp.libraryPaths: googleCommon.getChildPath(qbs, googletestDir, "lib")

    files: [
        "main.cpp",
        "tst_client.cpp",
        "tst_server.cpp",
        "tst_ttftpreceiver.cpp",
        "tst_ttftpsender.cpp",
    ]

    Group
    {
        name: "Sourcefiles"
        prefix: FileInfo.joinPaths(projectSourcePath)
    files: [
            "tftpmessages.cpp",
            "tftpmessages.h",
            "tftpclient.cpp",
            "tftpreceiver.cpp",
            "tftpsender.cpp",
            "tftpserver.cpp",
            "tftphelpdefs.cpp",
        ]
    }

}
