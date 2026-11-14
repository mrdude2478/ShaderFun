#pragma once
// Which shader comes next: favourites, "in order" or "random", for the auto-change timer and the L / R buttons.
// It always works on the list main.cpp passes in, which already holds only the shaders that may play right now
// (the Docked* folders are left out while the console is handheld), so docked-only shaders can never appear in handheld mode.

#include <string>
#include <vector>

namespace shaderpick {

enum Order { ORDER_LIST = 0, ORDER_RANDOM = 1 };
enum Scope { SCOPE_ALL = 0, SCOPE_FAVOURITES = 1 };

void load();                                    // reads favourites.txt and the saved settings

Order order();
void setOrder(Order o);
Scope scope();
void setScope(Scope s);

bool isFavourite(const std::string& shaderPath);
bool toggleFavourite(const std::string& shaderPath);   // returns the new state
int favouriteCount();

// The shader to show after (dir = +1) or before (dir = -1) 'current'. Returns an index into 'list'
// (or 'current' itself when there is nothing else to choose from).
int pickIndex(const std::vector<std::string>& list, int current, int dir);

// The same answer pickIndex would give, without using it up: nothing is recorded (no history, no "already shown").
// In random mode the next shader is chosen once and remembered, so peekIndex(+1) and the following
// pickIndex(+1) agree. This lets main.cpp prepare the next and previous shaders before they are needed.
int peekIndex(const std::vector<std::string>& list, int current, int dir);

void noteJump(const std::string& leavingPath);  // you picked a shader yourself: remember where you were (for "previous")
bool consumeFallbackNotice();                   // true once when "favourites only" had no favourite to use and showed all shaders

} // namespace shaderpick
