#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    SearchServer add_server = search_server;
    std::set<std::set<std::string_view>> worlds_in_document;
    size_t worlds_count = 0;

    for (const  auto& id : add_server) {
        worlds_in_document.insert(ExtractOfKeys(add_server.GetWordFrequencies(id)));
        if (worlds_in_document.size() == worlds_count) {
            search_server.RemoveDocument(id);
        }
        else {
            ++worlds_count;
        }
    }
}



