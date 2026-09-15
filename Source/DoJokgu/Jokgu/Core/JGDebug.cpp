#include "Jokgu/Core/JGDebug.h"

TAutoConsoleVariable<bool> CVarJGDrawDebug(
	TEXT("jg.Debug.Draw"),
	false,
	TEXT("Draws Jokgu debug helpers: court zones, hit reach, predicted ball path and ground contacts."),
	ECVF_Cheat);
