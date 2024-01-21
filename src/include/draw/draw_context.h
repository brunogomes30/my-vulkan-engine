#pragma once
#include <vector>
#include<draw/render_object.h>

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};