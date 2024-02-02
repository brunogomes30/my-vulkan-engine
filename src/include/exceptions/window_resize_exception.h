#pragma once
#include<iostream>
class WindowResizeException : public std::exception
{
public:
	const char* what() {
		return "Window resize exception";
	}
};