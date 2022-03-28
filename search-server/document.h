//Вставьте сюда своё решение из урока «‎Очередь запросов».‎
#pragma once
#include <sstream>

struct Document {
    Document();
    
    Document(int id, double relevance, int rating) ;

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

void PrintDocument(const Document& document) ;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

std::ostream& operator<<(std::ostream& out, const Document& document) ;