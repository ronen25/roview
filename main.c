#include <raylib.h>
#include <raymath.h>
#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "renderer.h"
#include "pdfpage.h"

typedef struct {
    PDFRenderer *pdf_renderer;

    // Viewport
    int window_width;
    int window_height;
    float scroll_y;           // Current scroll position
    float target_scroll_y;    // Target scroll position for smooth scrolling

    int current_page_index;
    int total_pages;
    bool page_rendered;
    PDFPage current_page;
} PDFViewer;

void RenderCurrentPage(PDFViewer *viewer);

PDFViewer* CreatePDFViewer(const char *filename, int width, int height) {
    PDFViewer *viewer = malloc(sizeof(PDFViewer));
    if (!viewer) return NULL;

    memset(viewer, 0, sizeof(PDFViewer));

    viewer->window_width = width;
    viewer->window_height = height;
    viewer->scroll_y = 0.0f;
    viewer->target_scroll_y = 0.0f;
    viewer->page_rendered = false;

    // Initialize PDF renderer
    printf("Debug: Initializing PDF renderer for file: %s\n", filename);
    viewer->pdf_renderer = PDFRendererNew(filename);
    if (!viewer->pdf_renderer) {
        printf("Error: Failed to create PDF renderer\n");
        free(viewer);
        return NULL;
    }

    viewer->total_pages = PDFRendererGetPageCount(viewer->pdf_renderer);
    printf("Debug: PDF loaded successfully - %d pages\n", viewer->total_pages);

    if (viewer->total_pages <= 0) {
        printf("Error: PDF has no pages\n");
        PDFRendererFree(viewer->pdf_renderer);
        free(viewer);
        return NULL;
    }

    // Render initial page
    printf("Debug: Rendering initial page\n");
    RenderCurrentPage(viewer);

    return viewer;
}

void RenderCurrentPage(PDFViewer *viewer) {
    // Clean up old texture
    if (viewer->current_page.texture.id != 0) {
        UnloadTexture(viewer->current_page.texture);
        viewer->current_page.texture.id = 0;
    }

    printf("Debug: Rendering page %d at default zoom to get dimensions\n", viewer->current_page_index);

    // Set zoom to 1.0 first to get base dimensions
    PDFRendererSetZoom(viewer->pdf_renderer, 1.0f);

    int original_width, original_height, stride;
    unsigned char *temp_data = PDFRendererRenderPage(viewer->pdf_renderer, viewer->current_page_index,
                                                       &original_width, &original_height, &stride);

    if (!temp_data) {
        printf("Error: Failed to render page %d at default zoom\n", viewer->current_page_index);
        return;
    }

    printf("Debug: Original page dimensions: %dx%d\n", original_width, original_height);
    free(temp_data); // Free the temporary render data

    float zoom = (float)(viewer->window_width - 20) / (float)original_width; // 20px padding
    printf("Debug: Calculated zoom factor: %.2f\n", zoom);

    PDFRendererSetZoom(viewer->pdf_renderer, zoom);

    // Render PDF page to memory at the correct zoom
    int width, height;
    unsigned char *data = PDFRendererRenderPage(viewer->pdf_renderer,
        viewer->current_page_index, &width, &height, &stride);

    if (!data) {
        printf("Error: Failed to render page %d at zoom %.2f\n", viewer->current_page_index, zoom);
        return;
    }

    printf("Debug: Final rendered page size: %dx%d at zoom %.2f\n", width, height, zoom);

    // Create a copy of the data for Raylib
    unsigned char *texture_data = malloc(width * height * 3);
    if (!texture_data) {
        printf("Error: Failed to allocate texture data\n");
        free(data);
        return;
    }

    memcpy(texture_data, data, width * height * 3);

    Image image = {
        .data = texture_data,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
        .mipmaps = 1
    };

    viewer->current_page.texture = LoadTextureFromImage(image);
    viewer->current_page.width = width;
    viewer->current_page.height = height;
    viewer->page_rendered = true;

    printf("Debug: Texture created successfully - ID: %d\n", viewer->current_page.texture.id);

    free(texture_data);
    free(data);
}

void UpdateInput(PDFViewer *viewer) {
    // Handle mouse wheel for scrolling
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        float scroll_speed = 50.0f;
        viewer->target_scroll_y += wheel * scroll_speed;

        // Clamp scroll position
        float max_scroll = fmaxf(0.0f, viewer->current_page.height - viewer->window_height);
        if (viewer->target_scroll_y < 0.0f) viewer->target_scroll_y = 0.0f;
        if (viewer->target_scroll_y > max_scroll) viewer->target_scroll_y = max_scroll;
    }

    // Keyboard navigation for scrolling
    float scroll_speed = 300.0f * GetFrameTime();
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
        viewer->target_scroll_y -= scroll_speed;
    }
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
        viewer->target_scroll_y += scroll_speed;
    }

    // Page up/down scrolling
    if (IsKeyPressed(KEY_PAGE_UP)) {
        viewer->target_scroll_y -= viewer->window_height * 0.9f;
    }
    if (IsKeyPressed(KEY_PAGE_DOWN)) {
        viewer->target_scroll_y += viewer->window_height * 0.9f;
    }

    // Clamp scroll position
    float max_scroll = fmaxf(0.0f, viewer->current_page.height - viewer->window_height);
    if (viewer->target_scroll_y < 0.0f) viewer->target_scroll_y = 0.0f;
    if (viewer->target_scroll_y > max_scroll) viewer->target_scroll_y = max_scroll;

    // Page navigation (discrete page changes)
    bool page_changed = false;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        if (viewer->current_page_index < viewer->total_pages - 1) {
            viewer->current_page_index++;
            page_changed = true;
        }
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        if (viewer->current_page_index > 0) {
            viewer->current_page_index--;
            page_changed = true;
        }
    }

    // Go to specific page with number keys
    for (int i = KEY_ONE; i <= KEY_NINE; i++) {
        if (IsKeyPressed(i)) {
            int page_num = i - KEY_ONE;
            if (page_num < viewer->total_pages) {
                viewer->current_page_index = page_num;
                page_changed = true;
            }
        }
    }

    // Go to first/last page
    if (IsKeyPressed(KEY_HOME)) {
        if (viewer->current_page_index != 0) {
            viewer->current_page_index = 0;
            page_changed = true;
        }
        viewer->target_scroll_y = 0.0f;
    }
    if (IsKeyPressed(KEY_END)) {
        if (viewer->current_page_index != viewer->total_pages - 1) {
            viewer->current_page_index = viewer->total_pages - 1;
            page_changed = true;
        }
    }

    // Re-render page if changed
    if (page_changed) {
        RenderCurrentPage(viewer);
        // Reset scroll position for new page
        viewer->target_scroll_y = 0.0f;
        viewer->scroll_y = 0.0f;
    }

    // Reset scroll to top
    if (IsKeyPressed(KEY_R)) {
        viewer->target_scroll_y = 0.0f;
    }

    // Smooth scrolling interpolation
    float lerp_speed = 15.0f * GetFrameTime();
    viewer->scroll_y = Lerp(viewer->scroll_y, viewer->target_scroll_y, lerp_speed);
}

void DrawPDFViewer(PDFViewer *viewer) {
    BeginDrawing();
    ClearBackground((Color){50, 50, 50, 255});

    if (viewer->page_rendered && viewer->current_page.texture.id != 0) {
        // Page position (left-aligned, scrolled vertically)
        Vector2 page_pos = {
            10.0f,  // Small left margin
            10.0f - viewer->scroll_y  // Apply scroll offset with top margin
        };

        // Draw page
        DrawTextureV(viewer->current_page.texture, page_pos, WHITE);

        // Draw page border
        Rectangle page_rect = {
            page_pos.x,
            page_pos.y,
            viewer->current_page.width,
            viewer->current_page.height
        };
        DrawRectangleLinesEx(page_rect, 2, (Color){200, 200, 200, 255});
    } else {
        // Debug: Show if page is not rendered
        DrawText("PDF page not rendered", 10, 50, 20, RED);
        char debug_text[256];
        sprintf(debug_text, "Rendered: %s, Texture ID: %d",
                viewer->page_rendered ? "true" : "false", viewer->current_page.texture.id);
        DrawText(debug_text, 10, 80, 20, WHITE);
    }

    EndDrawing();
}

void DestroyPDFViewer(PDFViewer *viewer) {
    if (!viewer) return;

    if (viewer->current_page.texture.id != 0) {
        UnloadTexture(viewer->current_page.texture);
    }

    if (viewer->pdf_renderer) {
        PDFRendererFree(viewer->pdf_renderer);
    }

    free(viewer);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input.pdf>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const int screenWidth = 800;
    const int screenHeight = 1200;

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "roview");
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    PDFViewer *viewer = CreatePDFViewer(argv[1], screenWidth, screenHeight);
    if (!viewer) {
        fprintf(stderr, "Failed to create PDF viewer\n");
        CloseWindow();
        return EXIT_FAILURE;
    }

    while (!WindowShouldClose() && !IsKeyPressed(KEY_ESCAPE)) {
        // Handle window resize
        if (IsWindowResized()) {
            viewer->window_width = GetScreenWidth();
            viewer->window_height = GetScreenHeight();

            RenderCurrentPage(viewer);
        }

        UpdateInput(viewer);
        DrawPDFViewer(viewer);
    }

    DestroyPDFViewer(viewer);
    CloseWindow();

    return 0;
}