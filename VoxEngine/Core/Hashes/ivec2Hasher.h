#pragma once
#include <glm/vec2.hpp>

struct ivec2Hasher
{
public:
	size_t operator()(const glm::ivec2& other) const noexcept;
};

