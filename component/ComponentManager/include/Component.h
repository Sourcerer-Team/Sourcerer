#pragma once

namespace ComponentManager {

class Component {
public:
    Component() {}
    virtual ~Component() {}
    virtual void start() = 0;
    virtual void stop() = 0;
};

}
