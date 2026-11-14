#include "Globals.h"
#include "Clock.h"

/*
Clock::Clock() {
	QueryPerformanceCounter(&start_time);
	QueryPerformanceFrequency(&frequency);
}

void Clock::Reset() {
	QueryPerformanceCounter(&start_time);
	elapsed_time = 0;
}

void Clock::Tick(void) {
	LARGE_INTEGER end_time;

	QueryPerformanceCounter(&end_time);
	elapsed_time = (start_time.QuadPart - end_time.QuadPart) / frequency.QuadPart;
}

double Clock::Now(void) {
	LARGE_INTEGER end_time;
	return static_cast<double>((start_time.QuadPart - end_time.QuadPart) / frequency.QuadPart);
}

double Clock::DeltaTime(void) const {
	return 0;
}

double Clock::FPS(void) const {
	return 0;
}
*/