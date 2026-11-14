#pragma once

#include "Module.h"

class Exercise1 : public Module
{
public:
	Exercise1() = default;
	~Exercise1() = default;

	bool init(IDXGIFactory *factory);
	bool cleanUp();
private:

};
