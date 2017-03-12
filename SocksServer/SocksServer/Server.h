#pragma once

#include <stdio.h>
#include <WinSock.h>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")

namespace ReverseSocksServer
{
   BOOL Start(INT port);
}