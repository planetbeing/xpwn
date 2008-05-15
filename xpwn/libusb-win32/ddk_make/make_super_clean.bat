@echo off

call make_clean.bat

if exist *.exe del /q *.exe
if exist *.dll del /q *.dll
if exist *.sys del /q *.sys 
if exist *.lib del /q *.lib
if exist *.log del /q *.log 
if exist *.wrn del /q *.wrn 
if exist *.err del /q *.err
if exist manifest.txt del /q manifest.txt