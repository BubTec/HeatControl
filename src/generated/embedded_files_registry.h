// Auto-generated embedded file registry
#pragma once

#include <Arduino.h>

#include "embedded_index_html.h"
#include "embedded_logo_png.h"

struct EmbeddedFile {
    const char *path;
    const char *content_type;
    const uint8_t *data;
    size_t size;
};

extern const EmbeddedFile embedded_files[];
extern const size_t embedded_files_count;
const EmbeddedFile *findEmbeddedFile(const char *path);
