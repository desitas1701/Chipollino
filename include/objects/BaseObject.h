#pragma once
#include <string>

class BaseObject {
  public:
	virtual std::string to_txt() = 0;
};