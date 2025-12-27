#ifndef PERSIAN_SHAPER_H
#define PERSIAN_SHAPER_H

#include <cstdint>
#include <vector>

void ShapePersianText(std::vector<std::int32_t>& codepoints);
bool IsPersianLetter(std::int32_t codepoint);

#endif // PERSIAN_SHAPER_H
