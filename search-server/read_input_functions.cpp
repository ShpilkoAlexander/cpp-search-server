//Вставьте сюда своё решение из урока «‎Очередь запросов».‎
#include "read_input_functions.h"
#include <iostream>
#include <string>

std::string ReadLine() {
    std::string s;
    std::getline(std::cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}