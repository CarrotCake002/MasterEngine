#pragma once

#include "Module.h"
#include "ImGuiPass.h"

class ImGuiModule
{
	ImGuiModule(HWND hWnd) : hWnd(hWnd) {}


	bool init(void);
	void update(void);
	bool cleanUp(void);

	// Commands
	void resetCommands(void);
	void submitCommands(void);

	// render
	void render(void);
	void preRender(void);
	void postRender(void);
	void renderImGui(void);

private:
	// ImGUI
	std::unique_ptr <ImGuiPass> imgui;

	HWND hWnd;
};

