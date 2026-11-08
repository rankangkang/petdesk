#pragma once
#include <QString>

namespace petdesk {
class PetLibrary;
// Makes an optional distribution's pets available and protects them while it is installed.
void seedBundledPets(const QString &directory, PetLibrary &library);
}
