#pragma once
// Is Wi-Fi on and connected to a network? (implemented in main.cpp, which starts the system's network service)
// Also true when the system cannot tell, so nothing is blocked by mistake.
bool wifiConnected();
