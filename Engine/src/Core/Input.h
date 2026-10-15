#pragma once
#include "GLFW/glfw3.h"
#include <EASTL/bitset.h>

class Input
{
public:
	void init(GLFWwindow *window);

	void update()
	{
		pressed_keys.reset();
		released_keys.reset();
		pressed_mouse_keys.reset();
		released_mouse_keys.reset();
		mouse_wheel = 0.0f;
	}

	bool isKeyDown(int key) { return down_keys.test(key); }
	bool isKeyPressed(int key) { return pressed_keys.test(key); }
	bool isKeyReleased(int key) { return released_keys.test(key); }

	bool isMouseKeyDown(int button) { return down_mouse_keys.test(button); }
	bool isMouseKeyPressed(int button) { return pressed_mouse_keys.test(button); }
	bool isMouseKeyReleased(int button) { return released_mouse_keys.test(button); }

	glm::vec2 getMousePosition() { return mouse_position; }
	float getMouseWheel() { return mouse_wheel; }

	bool isCursorDisabled() { return glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED; }
	void setCursorDisabled(bool disabled) { glfwSetInputMode(window, GLFW_CURSOR, disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL); }

private:
	static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
	static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
	static void cursor_position_callback(GLFWwindow *window, double x, double y);
	static void scroll_callback(GLFWwindow *window, double x_offset, double y_offset);

	GLFWwindow *window = nullptr;

	eastl::bitset<GLFW_KEY_LAST + 1> down_keys;
	eastl::bitset<GLFW_KEY_LAST + 1> pressed_keys;
	eastl::bitset<GLFW_KEY_LAST + 1> released_keys;
	eastl::bitset<GLFW_MOUSE_BUTTON_LAST + 1> down_mouse_keys;
	eastl::bitset<GLFW_MOUSE_BUTTON_LAST + 1> pressed_mouse_keys;
	eastl::bitset<GLFW_MOUSE_BUTTON_LAST + 1> released_mouse_keys;
	glm::vec2 mouse_position = glm::vec2(0.0f);
	float mouse_wheel = 0.0f;
};

extern Input gInput;
