#include "askdocs/types.hpp"

namespace askdocs {

bool Rect::contains(int row, int col) const noexcept {
    return row >= top && row < top + height && col >= left && col < left + width;
}

}  // namespace askdocs
