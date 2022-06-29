//Вставьте сюда своё решение из урока «‎Очередь запросов».‎
#pragma once
#include <string>
#include <vector>
#include <set>

std::vector<std::string_view> SplitIntoWords(std::string_view str) ;

template <typename StringContainer>
std::set<std::string_view, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string_view, std::less<>> non_empty_strings ;
    for (std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

