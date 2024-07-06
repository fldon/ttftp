import qbs
Project
{
    name: "TTFTP"
    references: [
        "app/app.qbs"
    ]

SubProject {
        filePath: "ttftpTest/ttftpTest.qbs"
}
}
