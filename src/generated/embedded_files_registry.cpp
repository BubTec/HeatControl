// Auto-generated embedded file registry implementation
#include "embedded_files_registry.h"

#include <cstring>

const EmbeddedFile embedded_files[] = {
    { "/index.html", "text/html", embedded_index_html_data, embedded_index_html_size },
    { "/LOGO.png", "image/png", embedded_logo_png_data, embedded_logo_png_size },
};

const size_t embedded_files_count = sizeof(embedded_files) / sizeof(embedded_files[0]);

const EmbeddedFile *findEmbeddedFile(const char *path) {
    if (path == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; i < embedded_files_count; ++i) {
        if (std::strcmp(embedded_files[i].path, path) == 0) {
            return &embedded_files[i];
        }
    }
    return nullptr;
}
