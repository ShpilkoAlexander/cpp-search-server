#pragma once

#include "string_processing.h"
#include "concurrent_map.h"
#include "document.h"

#include <stdexcept>
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>
#include <tuple>
#include <set>
#include <map>
#include <execution>


const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6; // точность сравнения релевантности (double)
const int MAP_BUCKETS = 101;



class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) ;
    explicit SearchServer(const std::string& stop_words_text) ;
    explicit SearchServer(const std::string_view stop_words_text) ;

    void AddDocument(int document_id,
                     std::string_view document,
                     DocumentStatus status,
                     const std::vector<int>& ratings) ;

    template <typename Predicate>
    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query,
                     Predicate document_predicate) const;

    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query,
                     DocumentStatus status) const;

    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query) const;

    template <typename ExecutionPolicy, typename Predicate>
    std::vector<Document>
    FindTopDocuments(const ExecutionPolicy& policy,
                     const std::string_view raw_query,
                     Predicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(const ExecutionPolicy& policy,
                     const std::string_view raw_query,
                     DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(const ExecutionPolicy& policy,
                     const std::string_view raw_query) const;

    int GetDocumentCount() const ;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const ;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy& sequen, std::string_view raw_query, int document_id) const ;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&  paral, std::string_view raw_query, int document_id) const ;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename Policy>
    void RemoveDocument(Policy policy, int document_id);

private:

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };


    const std::set<std::string_view, std::less<>> stop_words_;
    std::set<std::string, std::less<>> words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;

    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_ids_with_word_;

    bool IsStopWord(std::string_view word) const ;

    static bool IsValidWord(std::string_view word) ;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const ;

    static int ComputeAverageRating(const std::vector<int>& ratings) ;

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const ;

//    struct Query {
//        std::set<std::string> plus_words;
//        std::set<std::string> minus_words;
//    };

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    template <typename ExecutionPolicy>
    Query ParseQuery(const ExecutionPolicy& policy, const std::string_view text, const bool make_unique = true) const;

//    QueryVector ParseQueryVector(const std::string& text) const ;

    double ComputeWordInverseDocumentFreq(const std::string_view word) const ;

    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const Query& query, Predicate document_predicate) const;

    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& policy, const Query& query, Predicate document_predicate) const;

    template <typename Predicate>
    std::vector<Document>
    FindAllDocuments(const std::execution::parallel_policy& policy, const Query& query, Predicate document_predicate) const;
};


template <typename ExecutionPolicy>
SearchServer::Query
SearchServer::ParseQuery(const ExecutionPolicy& policy, const std::string_view text, const bool make_unique) const {
    Query result;
    bool has_minus = false;
    bool has_plus = false;
    const auto& words = SplitIntoWords(text);
    for_each(words.begin(), words.end(), [this, &has_minus, &has_plus, &result] (std::string_view word) {
                 const auto query_word = ParseQueryWord(word);
                 if (!query_word.is_stop) {
                     if (query_word.is_minus) {
                         has_minus = true;
                         result.minus_words.push_back(query_word.data);
                     } else {
                         has_plus = true;
                         result.plus_words.push_back(query_word.data);
                     }
                 }
             });

    if (!make_unique) {
        return result;
    }

    if (has_minus) {
        std::sort(policy, result.minus_words.begin(), result.minus_words.end());
        auto it = std::unique(policy, result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(it, result.minus_words.end());
    }
    if (has_plus) {
        std::sort(policy, result.plus_words.begin(), result.plus_words.end());
        auto it = std::unique(policy, result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(it, result.plus_words.end());
    }
    return result;
}

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename Predicate>
std::vector<Document>
SearchServer::FindTopDocuments(const std::string_view raw_query, Predicate document_predicate) const {
    const auto query = ParseQuery(std::execution::seq, raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);
    std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
                  if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query,Predicate document_predicate) const {
    const Query query = ParseQuery(policy, raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, document_predicate);
    std::sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
                  if (std::abs(lhs.relevance - rhs.relevance) <
                      EPSILON) {
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


template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,[status](int document_id, DocumentStatus document_status, int rating) {
                        return document_status == status;
                    });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

// FindAllDocuments
template <typename Predicate>
std::vector<Document>
SearchServer::FindAllDocuments(const Query& query,
                               Predicate document_predicate) const {
    std::map<int, double> document_to_relevance;

    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        const double inverse_document_freq =
                     ComputeWordInverseDocumentFreq(word);

        for (const auto [document_id, term_freq] :
             word_to_document_freqs_.at(word)) {
            const auto& document_data =
                documents_.at(document_id);
            if (document_predicate(document_id,
                document_data.status,
                document_data.rating)) {
                document_to_relevance[document_id] +=
                    term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        for (const auto [document_id, _] :
            word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] :
         document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance,
              documents_.at(document_id).rating });
    }

    return matched_documents;
}

// FindAllDocuments sequenced_policy
template <typename Predicate>
std::vector<Document>
SearchServer::FindAllDocuments(
              const std::execution::sequenced_policy& policy,
              const Query& query,
              Predicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

// FindAllDocuments parallel_policy
template <typename Predicate>
std::vector<Document>
SearchServer::FindAllDocuments(
              const std::execution::parallel_policy& policy,
              const Query& query,
              Predicate document_predicate) const {
    ConcurrentMap<int, double> relevances(MAP_BUCKETS);

    for_each (policy,
              query.plus_words.begin(),
              query.plus_words.end(),
              [this, &relevances, &document_predicate]
              (std::string_view word) {
                  if (word_to_document_freqs_.count(word) == 0) {
                      return;
                  }
                  const double
                  idf = ComputeWordInverseDocumentFreq(word);

                  for (const auto& [id, freq]
                       : word_to_document_freqs_.at(word)) {
                      const DocumentData doc = documents_.at(id);
                      if (document_predicate(id, doc.status,
                                             doc.rating)) {
                          relevances[id].ref_to_value +=
                              freq * idf;
                      }
                  }
              });

    for_each (policy,
              query.minus_words.begin(),
              query.minus_words.end(),
              [this, &relevances]
              (std::string_view word) {
                  if (word_to_document_freqs_.count(word) == 0) {
                      return;
                  }

                  for (const auto& [id, _]
                       : word_to_document_freqs_.at(word)) {
                      relevances.Erase(id);
                  }
              });

    std::vector<Document> matched_documents;
    for (const auto [id, relevance] :
         relevances.BuildOrdinaryMap()) {
        matched_documents.push_back(
            { id, relevance,
              documents_.at(id).rating });
    }

    return matched_documents;
}


template <typename Policy>
void SearchServer::RemoveDocument(Policy policy, const int document_id){
    if (!document_ids_.count(document_id)) {
             return;
    }

    std::vector<const std::string*> words_to_delete(document_ids_with_word_.at(document_id).size());
    std::transform(policy,
                   document_ids_with_word_.at(document_id).cbegin(), document_ids_with_word_.at(document_id).cend(),
                   words_to_delete.begin(),
                    [] (const std::pair<const std::string, double>& words_freq) {
                     return &words_freq.first;
                    }
    );

    std::for_each(policy,
                  words_to_delete.cbegin(), words_to_delete.cend(),
                  [this, document_id] (const auto& word) {
         word_to_document_freqs_.at(*word).erase(document_id);
    });

    document_ids_.erase(document_id);
    document_ids_with_word_.erase(document_id);
    documents_.erase(document_id);
}
