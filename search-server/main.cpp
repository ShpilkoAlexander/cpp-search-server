#include "document.h"
#include "paginator.h"
#include "read_input_functions.h"
#include "request_queue.h"
#include "search_server.h"
#include "string_processing.h"
#include "remove_duplicates.h"
#include <iostream>

using namespace std;

int main() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    search_server.AddDocument(6, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});


    for (auto document_id : search_server) {
        std::cout << document_id << endl;
    }
    RemoveDuplicates(search_server);
    for (auto document_id : search_server) {
        std::cout << document_id << endl;
    }
    return 0;
}
