#!/bin/sh
gcc -g -Wall -I./QuicktimeSDK/CIncludes idevice.c windows.c utils.c ./QuicktimeSDK/Libraries/QTMLClient.lib -o idevice
