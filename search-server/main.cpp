#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <tuple>
#include <sstream>
#include <numeric>
using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};



enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    template <typename IsActual>
    vector<Document> FindTopDocuments(const string& raw_query, IsActual is_actual) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, is_actual);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const{
        return FindTopDocuments(raw_query, DocumentStatus :: ACTUAL);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const{
        return FindTopDocuments(raw_query,
               [status](int document_id, DocumentStatus status_document, int rating) {
                   return status == status_document;
               }
        );
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }



    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if(text.empty()) {
            return {};
        }

        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        if (word_to_document_freqs_.count(word)) {
            return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
        }
        else {
                return 0.0;
        }
    }


    template<typename IsActual>
    vector<Document> FindAllDocuments(const Query& query, IsActual is_actual) const{
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (is_actual(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};




void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}



template <typename El>
ostream& operator<< (ostream& output, vector<El> elements) {
    bool isFirst = true;
    output << "[";
    for(const auto& element : elements) {
        if(isFirst) {
            output << element;
            isFirst = false;
            continue;
        }
        output << ", " << element;
    }
    output << "]";

    return output;
}

template <typename El>
ostream& operator<< (ostream& output, set<El> elements) {
    bool isFirst = true;
    output << "{";
    for(const auto& element : elements) {
        if(isFirst) {
            output << element;
            isFirst = false;
            continue;
        }
        output << ", " << element;
    }
    output << "}";

    return output;
}

template <typename Key, typename Value>
ostream& operator<< (ostream& output, map<Key, Value> elements) {
    bool isFirst = true;
    output << "{";
    for(const auto& [key, value] : elements) {
        if(isFirst) {
            output << key << ": " << value;
            isFirst = false;
            continue;
        }
        output << ", " << key << ": " << value;
    }
    output << "}";

    return output;
}


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void Assert(bool value, const string& str_func,
            const string& file, const string& function,
            const unsigned line,
            const string& hint) {
    if(!value) {
        cout << file <<"("s << line << "): "s << function << ": "s;
        cout << "ASSERT(" << str_func << ") failed."s;
        if(!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) Assert(expr, #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) Assert(expr, #expr, __FILE__, __FUNCTION__, __LINE__, hint)

template <typename T>
void RunTestImpl(const T& func, const string& str_func) {
    func();
        cerr << str_func << " OK" << endl;


}

#define RUN_TEST(func) RunTestImpl((func), #func)




// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/
//Тестирование исключения документов с минус словами
void TestExcludesDocumentWithMinusWord() {
    const int doc_id = 42;
    const string content = "grey cat with collar in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat"s).size() == 1);
        ASSERT(server.FindTopDocuments("cat -in"s).empty());
    }
}

//Тестирование того, что при Матчинге возврящется пустой вектор слов, если есть минус слово
void TestSearchWordWithMinusWord() {
    const int doc_id = 42;
    const string content = "grey cat with collar in the city"s;
    const vector<int> ratings = {1, 2, 3};

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    {
        const auto [match_word, document_status] = server.MatchDocument("cat dog city"s, 42);
        ASSERT(match_word.size() == 2);
    }
    {
        const auto [match_word, document_status] = server.MatchDocument("cat dog city -in"s, 42);
        ASSERT(match_word.empty());
    }
}

//Добавление нескольких документов;
void AddDocuments(const vector<int>& doc_id, const vector<string>& content, const vector<vector<int>>& ratings, SearchServer& server) {
    for(size_t i = 0; i < doc_id.size(); i++) {
        server.AddDocument(doc_id[i], content[i], DocumentStatus::ACTUAL, ratings[i]);
    }
}

// Тестирование сортировки по релевантности
void TestSortingByRelevance() {
    const vector<int> doc_id {1, 2, 3};
    const vector<string> content {"grey cat with collar in the city"s,
                                  "grey dog with in the town"s,
                                  "black pig with collar village"s
                                 };
    const vector<vector<int>> ratings {{1, 2, 3},
                                       {2, 5, 3},
                                       {1, 6, 8}
                                      };
    SearchServer server;
    AddDocuments(doc_id, content, ratings, server);

    vector<Document> finded_documents = server.FindTopDocuments("grey dog collar"s);
    for(size_t i = 1; i < finded_documents.size(); i++) {
        ASSERT(finded_documents[i-1].relevance >= finded_documents[i].relevance);
    }
}

    // Тестирование вычисления релевантности
void TestComputeRelevance() {
    const vector<int> doc_id {1, 2, 3};
    const vector<string> content {"white cat fashionable collar"s,
                                  "fluffy cat fluffy tail"s,
                                  "groomed dog expressive eyes"s
                                 };
    const vector<vector<int>> ratings {{1, 2, 3},
                                       {2, 5, 3},
                                       {1, 6, 8}
                                      };
    SearchServer server;
    AddDocuments(doc_id, content, ratings, server);

    vector<double> relevance_doc;
    relevance_doc.push_back(log(server.GetDocumentCount() * 1.0 / 2) * (1.0 / 4));
    relevance_doc.push_back(log(server.GetDocumentCount() * 1.0 / 1) * (2.0 / 4)
                             + log(server.GetDocumentCount() * 1.0 / 2) * (1.0 / 4));
    relevance_doc.push_back(log(server.GetDocumentCount() * 1.0 / 1) * (1.0 / 4));

    sort(relevance_doc.begin(), relevance_doc.end(), [] (double lhs, double rhs) {
        return lhs > rhs;
    });

    vector<Document> finded_documents = server.FindTopDocuments("fluffy groomed cat");
    const double EPSILON = 1e-6;
    for(size_t i = 0; i < relevance_doc.size(); i++) {
        ASSERT(abs(finded_documents[i].relevance - relevance_doc[i]) < EPSILON);
    }
}

//Тестирование вычисления рейтинга документов
void TestComputeRating() {
    const int doc_id = 1;
    const string content = "grey cat"s;
    const vector<int> ratings = {3, 1, 9};

    int rating = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    vector<Document> doc = server.FindTopDocuments("grey");
    ASSERT_EQUAL(doc[0].rating, rating);
}

//Тестирование фильтрация результатов поиска с использованием предиката
void TestFilteringResults() {
    const vector<int> doc_id {1, 2, 3, 4};
    const vector<string> content {"grey cat"s,
                                  "grey dog"s,
                                  "grey pig",
                                  "grey frog"
                                 };
    const vector<vector<int>> ratings {{0, 1, 2},
                                       {1, 2, 3},
                                       {2, 3, 4},
                                       {3, 4, 5}
                                      };
    SearchServer server;
    AddDocuments(doc_id, content, ratings, server);

    for (const Document& document : server.FindTopDocuments("grey"s,
                        [](int document_id, DocumentStatus status, int rating) {
                            return document_id % 2 == 0;
                        })) {
        ASSERT(document.id % 2 == 0);
    }

    for (const Document& document : server.FindTopDocuments("grey"s,
                        [](int document_id, DocumentStatus status, int rating) {
                            return rating > 2;
                        })) {
        ASSERT(document.rating > 2);
    }

    for (const Document& document : server.FindTopDocuments("grey"s,
                        [](int document_id, DocumentStatus status, int rating) {
                            return status == DocumentStatus::BANNED;
                        })) {
        ASSERT_EQUAL(document.id, doc_id[2]);
    }
}

//Тестирование поиска документов с заданным статусом
void TestSearchWithStatus() {
    const vector<int> doc_id {1, 2, 3};
    const vector<string> content {"grey cat"s,
                                  "grey dog"s,
                                  "grey pig",
                                 };
    const vector<vector<int>> ratings {{0, 1, 2},
                                       {1, 2, 3},
                                       {2, 3, 4},
                                      };
    SearchServer server;
    AddDocuments(doc_id, content, ratings, server);
    int id_banned_doc = 4;
    server.AddDocument(id_banned_doc, "grey frog", DocumentStatus::BANNED, {1, 2, 3});

    ASSERT_EQUAL(server.FindTopDocuments("grey", DocumentStatus::BANNED)[0].id, id_banned_doc);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludesDocumentWithMinusWord);
    RUN_TEST(TestSearchWordWithMinusWord);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestComputeRelevance);
    RUN_TEST(TestComputeRating);
    RUN_TEST(TestFilteringResults);
    RUN_TEST(TestSearchWithStatus);

}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}

