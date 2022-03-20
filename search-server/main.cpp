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
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
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
                if (is_actual(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)){
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


// ==================== для примера =========================


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}


/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/

template <typename El>
ostream& operator<< (ostream& output, vector<El> elements){
    bool isFirst = true;
    output << "[";
    for(const auto& element : elements){
        if(isFirst){
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
ostream& operator<< (ostream& output, set<El> elements){
    bool isFirst = true;
    output << "{";
    for(const auto& element : elements){
        if(isFirst){
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
ostream& operator<< (ostream& output, map<Key, Value> elements){
    bool isFirst = true;
    output << "{";
    for(const auto& [key, value] : elements){
        if(isFirst){
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
            const string& hint){
    if(!value){
        cout << file <<"("s << line << "): "s << function << ": "s;
        cout << "ASSERT(" << str_func << ") failed."s;
        if(!hint.empty()){
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
void TestExcludesDocumentWithMinusWord(){
    const int doc_id = 42;
    const string content = "grey cat with collar in the city"s;
    const vector<int> ratings = {1, 2, 3};

    //Проверка исключения документов с минус словами
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat").size() == 1);
        ASSERT(server.FindTopDocuments("cat -in").empty());
    }

    //Проверяем, что при Матчинге возврящется пустой вектор слов, если есть минус слово
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(get<0>(server.MatchDocument("cat dog city", 42)).size() == 2);
        ASSERT(get<0>(server.MatchDocument("cat dog city -in", 42)).empty());
    }
}


void TestingRelevance(){
    const int doc_id_1 = 1,
              doc_id_2 = 2,
              doc_id_3 = 3;
    const string content_1 = "grey cat with collar in the city"s,
                 content_2 = "grey dog with in the town"s,
                 content_3 = "black pig with collar village";
    const vector<int> ratings_1 = {1, 2, 3},
                      ratings_2 = {2, 5, 3},
                      ratings_3 = {1, 6, 8};
    // Тестирование сортировки по релевантности
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        vector <Document> finded_documents = server.FindTopDocuments("grey dog collar");
        vector<Document> sort_by_relevance = finded_documents;
        sort(sort_by_relevance.begin(), sort_by_relevance.end(), [] (const Document& lhs, const Document& rhs){
           return lhs.relevance > rhs.relevance;
        });
        ASSERT(server.FindTopDocuments("grey dog collar")[0].id == sort_by_relevance[0].id);
        ASSERT(server.FindTopDocuments("grey dog collar")[1].id == sort_by_relevance[1].id);
        ASSERT(server.FindTopDocuments("grey dog collar")[2].id == sort_by_relevance[2].id);

    }

    // Тестирование вычисления релевантности
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const double EPSILON = 1e-6;
        double dif_1 = server.FindTopDocuments( "grey dog collar")[0].relevance - 0.25068;
        double dif_2 = server.FindTopDocuments( "grey dog collar")[1].relevance - 0.115847;
        double dif_3 = server.FindTopDocuments( "grey dog collar")[2].relevance - 0.081093;

        ASSERT(abs(dif_1) < EPSILON);
        ASSERT(abs(dif_2) < EPSILON);
        ASSERT(abs(dif_3) < EPSILON);
    }

}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludesDocumentWithMinusWord);
    RUN_TEST(TestingRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
