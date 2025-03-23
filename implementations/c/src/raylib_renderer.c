#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "krb.h"

#define MAX_ELEMENTS 256
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define SCALE_FACTOR 1

typedef struct RenderElement {
    KrbElementHeader header;
    char* text;
    uint32_t bg_color;      // Background color (RGBA)
    uint32_t fg_color;      // Text/Foreground color (RGBA)
    uint32_t border_color;  // Border color (RGBA)
    uint8_t border_widths[4]; // Top, right, bottom, left
    struct RenderElement* parent;
    struct RenderElement* children[MAX_ELEMENTS];
    int child_count;
} RenderElement;

Color rgba_to_raylib_color(uint32_t rgba) {
    uint8_t r = (rgba >> 24) & 0xFF;
    uint8_t g = (rgba >> 16) & 0xFF;
    uint8_t b = (rgba >> 8) & 0xFF;
    uint8_t a = rgba & 0xFF;
    return (Color){r, g, b, a};
}

char* strip_quotes(const char* input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    if (len >= 2 && input[0] == '"' && input[len - 1] == '"') {
        char* stripped = malloc(len - 1);
        strncpy(stripped, input + 1, len - 2);
        stripped[len - 2] = '\0';
        return stripped;
    }
    return strdup(input);
}

void render_element(RenderElement* el, int parent_x, int parent_y, FILE* debug_file) {
    int x = parent_x + (el->header.pos_x * SCALE_FACTOR);
    int y = parent_y + (el->header.pos_y * SCALE_FACTOR);
    int width = (el->header.width * SCALE_FACTOR);
    int height = (el->header.height * SCALE_FACTOR);
    if (width < 10) width = 10;  // Minimum size in pixels
    if (height < 6) height = 6;

    // Inherit colors from parent if not set
    if (el->bg_color == 0 && el->parent) el->bg_color = el->parent->bg_color;
    if (el->fg_color == 0 && el->parent) el->fg_color = el->parent->fg_color;
    if (el->border_color == 0 && el->parent) el->border_color = el->parent->border_color;
    if (el->bg_color == 0) el->bg_color = 0x000000FF;     // Default black
    if (el->fg_color == 0) el->fg_color = 0xFFFFFFFF;     // Default white
    if (el->border_color == 0) el->border_color = 0x808080FF; // Default gray

    Color bg_color = rgba_to_raylib_color(el->bg_color);
    Color fg_color = rgba_to_raylib_color(el->fg_color);
    Color border_color = rgba_to_raylib_color(el->border_color);

    fprintf(debug_file, "DEBUG: Rendering element type=0x%02X at (%d, %d), size=%dx%d, text=%s\n",
            el->header.type, x, y, width, height, el->text ? el->text : "NULL");

    if (el->header.type == 0x01) { // Container
        // Draw background
        DrawRectangle(x, y, width, height, bg_color);

        // Draw borders
        int top = el->border_widths[0] * SCALE_FACTOR;
        int right = el->border_widths[1] * SCALE_FACTOR;
        int bottom = el->border_widths[2] * SCALE_FACTOR;
        int left = el->border_widths[3] * SCALE_FACTOR;

        if (top > 0)
            DrawRectangle(x, y, width, top, border_color);
        if (bottom > 0)
            DrawRectangle(x, y + height - bottom, width, bottom, border_color);
        if (left > 0)
            DrawRectangle(x, y, left, height, border_color);
        if (right > 0)
            DrawRectangle(x + width - right, y, right, height, border_color);

        // Render children
        for (int i = 0; i < el->child_count; i++) {
            render_element(el->children[i], x + left, y + top, debug_file);
        }
    } else if (el->header.type == 0x02 && el->text) { // Text
        int text_x = x + (2 * SCALE_FACTOR);  // Padding
        int text_y = y + (2 * SCALE_FACTOR);
        
        // Draw text background
        DrawRectangle(x, y, width, height, bg_color);
        
        // Draw text
        DrawText(el->text, text_x, text_y, 20, fg_color);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <krb_file>\n", argv[0]);
        return 1;
    }

    FILE* debug_file = fopen("krb_debug.log", "w");
    if (!debug_file) {
        debug_file = stderr;
        fprintf(debug_file, "DEBUG: Using stderr for logging\n");
    }

    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(debug_file, "Error: Could not open file %s\n", argv[1]);
        if (debug_file != stderr) fclose(debug_file);
        return 1;
    }

    KrbDocument doc = {0};
    if (!krb_read_document(file, &doc)) {
        fprintf(debug_file, "ERROR: Failed to parse KRB document\n");
        fclose(file);
        if (debug_file != stderr) fclose(debug_file);
        return 1;
    }

    RenderElement* elements = calloc(doc.header.element_count, sizeof(RenderElement));
    for (int i = 0; i < doc.header.element_count; i++) {
        elements[i].header = doc.elements[i];
        
        // Apply styles
        if (elements[i].header.style_id > 0 && elements[i].header.style_id <= doc.header.style_count) {
            KrbStyle* style = &doc.styles[elements[i].header.style_id - 1];
            for (int j = 0; j < style->property_count; j++) {
                KrbProperty* prop = &style->properties[j];
                if (prop->property_id == 0x01 && prop->value_type == 0x03 && prop->size == 4) {
                    elements[i].bg_color = *(uint32_t*)prop->value;
                } else if (prop->property_id == 0x02 && prop->value_type == 0x03 && prop->size == 4) {
                    elements[i].fg_color = *(uint32_t*)prop->value;
                } else if (prop->property_id == 0x03 && prop->value_type == 0x03 && prop->size == 4) {
                    elements[i].border_color = *(uint32_t*)prop->value;
                } else if (prop->property_id == 0x04 && prop->value_type == 0x01 && prop->size == 1) {
                    uint8_t width = *(uint8_t*)prop->value;
                    elements[i].border_widths[0] = elements[i].border_widths[1] = 
                    elements[i].border_widths[2] = elements[i].border_widths[3] = width;
                } else if (prop->property_id == 0x04 && prop->value_type == 0x08 && prop->size == 4) {
                    uint8_t* widths = (uint8_t*)prop->value;
                    elements[i].border_widths[0] = widths[0];
                    elements[i].border_widths[1] = widths[1];
                    elements[i].border_widths[2] = widths[2];
                    elements[i].border_widths[3] = widths[3];
                }
            }
        }

        // Apply element properties
        for (int j = 0; j < elements[i].header.property_count; j++) {
            KrbProperty* prop = &doc.properties[i][j];
            if (prop->property_id == 0x01 && prop->value_type == 0x03 && prop->size == 4) {
                elements[i].bg_color = *(uint32_t*)prop->value;
            } else if (prop->property_id == 0x02 && prop->value_type == 0x03 && prop->size == 4) {
                elements[i].fg_color = *(uint32_t*)prop->value;
            } else if (prop->property_id == 0x03 && prop->value_type == 0x03 && prop->size == 4) {
                elements[i].border_color = *(uint32_t*)prop->value;
            } else if (prop->property_id == 0x04 && prop->value_type == 0x01 && prop->size == 1) {
                uint8_t width = *(uint8_t*)prop->value;
                elements[i].border_widths[0] = elements[i].border_widths[1] = 
                elements[i].border_widths[2] = elements[i].border_widths[3] = width;
            } else if (prop->property_id == 0x04 && prop->value_type == 0x08 && prop->size == 4) {
                uint8_t* widths = (uint8_t*)prop->value;
                elements[i].border_widths[0] = widths[0];
                elements[i].border_widths[1] = widths[1];
                elements[i].border_widths[2] = widths[2];
                elements[i].border_widths[3] = widths[3];
            } else if (prop->property_id == 0x08 && prop->value_type == 0x04 && prop->size == 1) {
                uint8_t string_index = *(uint8_t*)prop->value;
                if (string_index < doc.header.string_count && doc.strings[string_index]) {
                    elements[i].text = strip_quotes(doc.strings[string_index]);
                }
            }
        }
    }

    // Build hierarchy
    for (int i = 0; i < doc.header.element_count; i++) {
        if (elements[i].header.child_count > 0) {
            int child_start = i + 1;
            for (int j = 0; j < elements[i].header.child_count && child_start + j < doc.header.element_count; j++) {
                elements[i].children[j] = &elements[child_start + j];
                elements[child_start + j].parent = &elements[i];
                elements[i].child_count++;
            }
        }
    }

    // Initialize Raylib
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "KRB Renderer (Raylib)");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // Render all root elements
        for (int i = 0; i < doc.header.element_count; i++) {
            if (!elements[i].parent) {
                render_element(&elements[i], 0, 0, debug_file);
            }
        }

        EndDrawing();
    }

    CloseWindow();

    // Cleanup
    for (int i = 0; i < doc.header.element_count; i++) {
        free(elements[i].text);
    }
    free(elements);
    krb_free_document(&doc);
    fclose(file);
    if (debug_file != stderr) fclose(debug_file);
    return 0;
}