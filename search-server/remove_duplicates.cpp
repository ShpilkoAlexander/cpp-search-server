#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    SearchServer add_server = search_server;
    for (auto iter = add_server.begin(); iter != add_server.end(); std::advance(iter, 1)) {
        auto finded_duplicates = std::find_if(next(iter), add_server.end(), [add_server, iter](int id) {
            return add_server.document_ids_with_word_.at(id) == add_server.document_ids_with_word_.at(*iter);
        });
        if (finded_duplicates != add_server.end()) {
            search_server.RemoveDocument(*finded_duplicates);
        }
    }
}


