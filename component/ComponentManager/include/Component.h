// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

namespace ComponentManager {

class Component {
public:
  Component() {}
  virtual ~Component() {}
  virtual void start() = 0;
  virtual void stop() = 0;
};

} // namespace ComponentManager
