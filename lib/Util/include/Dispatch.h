
#pragma once

#include <functional>

namespace Dispatch {

void dispatchToMainThread(std::function<void()> callback);

}
