#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace askdocs::nlp {

std::vector<std::string> tokenize(std::string_view text);
std::vector<float> bag_of_words(std::string_view text);

}  // namespace askdocs::nlp
