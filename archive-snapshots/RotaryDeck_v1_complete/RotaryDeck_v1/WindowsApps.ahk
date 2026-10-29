#Requires AutoHotkey v2.0
#SingleInstance Force
; Review/edit these local app paths before running. Never run elevated.
Apps := Map(
    "Code", A_LocalAppData "\Programs\Microsoft VS Code\Code.exe",
    "Chrome", A_ProgramFiles "\Google\Chrome\Application\chrome.exe",
    "Spotify", A_AppData "\Spotify\Spotify.exe",
    "Discord", A_LocalAppData "\Discord\Update.exe --processStart Discord.exe",
    "OBS", A_ProgramFiles "\obs-studio\bin\64bit\obs64.exe"
)
LaunchApp(name) {
    global Apps
    try {
        command := Apps[name]
        if name = "Discord" {
            exe := A_LocalAppData "\Discord\Update.exe"
            if !FileExist(exe)
                throw Error("Discord executable not found: " exe)
            Run('"' exe '" --processStart Discord.exe')
        } else {
            if !FileExist(command)
                throw Error("Executable not found: " command)
            Run('"' command '"')
        }
    } catch as err {
        MsgBox("Rotary Deck: " name " could not launch.`n`n" err.Message "`n`nEdit Apps in this script to match your installation.")
    }
}
^!+F1::LaunchApp("Code")
^!+F2::LaunchApp("Chrome")
^!+F3::LaunchApp("Spotify")
^!+F4::LaunchApp("Discord")
^!+F6::LaunchApp("OBS")
; Files uses Win+E directly; F5 is intentionally unassigned.
