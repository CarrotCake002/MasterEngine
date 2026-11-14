#pragma once

#include <Windows.h>
#include <profileapi.h>

class Clock
{
public:
	Clock() = default;
	~Clock() = default;

	void Reset(void);
	void Tick(void);
	double Now(void);

	double DeltaTime(void) const;
	double FPS(void) const;

private:
	LARGE_INTEGER start_time;
	LARGE_INTEGER frequency;

	double elapsed_time;
};

