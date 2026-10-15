#include "pch.h"
#include "Input.h"

Input gInput;

void Input::init(GLFWwindow *window)
{
	this->window = window;

	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	double x, y;
	glfwGetCursorPos(window, &x, &y);
	mouse_position = glm::vec2(x, y);

	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
}

void Input::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_UNKNOWN)
		return;

	if (action == GLFW_PRESS)
	{
		gInput.down_keys.set(key);
		gInput.pressed_keys.set(key);
	} else if (action == GLFW_RELEASE)
	{
		gInput.down_keys.reset(key);
		gInput.released_keys.set(key);
	}
}

void Input::mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		gInput.down_mouse_keys.set(button);
		gInput.pressed_mouse_keys.set(button);
	} else if (action == GLFW_RELEASE)
	{
		gInput.down_mouse_keys.reset(button);
		gInput.released_mouse_keys.set(button);
	}
}

void Input::cursor_position_callback(GLFWwindow *window, double x, double y)
{
	gInput.mouse_position = glm::vec2(x, y);
}

void Input::scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
	gInput.mouse_wheel += (float)y_offset;
}
