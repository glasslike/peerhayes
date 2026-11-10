@echo off
REM PeerHayes peer A (caller) — debug session.
REM COM10 is the peerhayesd end of the com0com pair. KittenMail on
REM FreeDOS uses the other end (COM3). The script lives next to the
REM example configs; the binary is the Release build one directory up.
"%~dp0..\build\peerhayesd.exe" --config "%~dp0peerhayes-com-a.conf" --debug
