#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "krb.h"

typedef struct {
    KrbElementHeader header;
    char* text;
} KrbElement;

void dump_bytes(const void* data, size_t size) {
    const unsigned char* bytes = (const unsigned char*)data;
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", bytes[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <krb_file>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        printf("Error: Could not open file %s\n", argv[1]);
        return 1;
    }

    // Get file size and dump content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("DEBUG: File size: %ld bytes\n", file_size);
    unsigned char* file_data = malloc(file_size);
    fread(file_data, 1, file_size, file);
    printf("DEBUG: Full file content:\n");
    dump_bytes(file_data, file_size);
    fseek(file, 0, SEEK_SET);
    free(file_data);

    // Read the document
    KrbDocument doc = {0};
    if (!krb_read_document(file, &doc)) {
        fclose(file);
        return 1;
    }

    // Convert to renderer-specific structure
    KrbElement* elements = calloc(doc.header.element_count, sizeof(KrbElement));
    for (int i = 0; i < doc.header.element_count; i++) {
        elements[i].header = doc.elements[i];
        for (int j = 0; j < doc.elements[i].property_count; j++) {
            KrbProperty* prop = &doc.properties[i][j];
            if (prop->property_id == 0x08 && prop->value_type == 0x04 && prop->size == 1 && doc.strings) {
                uint8_t string_index = *(uint8_t*)prop->value;
                if (string_index < doc.header.string_count && doc.strings[string_index]) {
                    elements[i].text = strdup(doc.strings[string_index]);
                    printf("DEBUG: Element text: '%s'\n", elements[i].text);
                }
            }
        }
    }

    // Render
    printf("\033[2J\033[H"); // Clear screen
    for (int i = 0; i < doc.header.element_count; i++) {
        if (elements[i].text) {
            int rows = elements[i].header.pos_y / 16 + 1;
            int cols = elements[i].header.pos_x / 8 + 1;
            printf("\033[%d;%dH%s", rows, cols, elements[i].text);
        }
    }
    printf("\033[25;1H\n");

    // Cleanup
    for (int i = 0; i < doc.header.element_count; i++) {
        if (elements[i].text) free(elements[i].text);
    }
    free(elements);
    krb_free_document(&doc);
    fclose(file);
    return 0;
}