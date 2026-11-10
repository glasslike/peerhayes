@echo off
REM PeerHayes peer B (answer) — debug session.
REM The script lives next to the example configs. The binary is the
REM Release build one directory up, so the paths do not depend on the
REM current directory.
"%~dp0..\build\peerhayesd.exe" --config "%~dp0peerhayes-com-b.conf" --debug
