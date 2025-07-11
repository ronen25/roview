#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

PDFRenderer* PDFRendererNew(const char *filename) {
    PDFRenderer *renderer = malloc(sizeof(PDFRenderer));
    if (!renderer) {
        fprintf(stderr, "Failed to allocate memory for PDF renderer\n");
        return NULL;
    }

    renderer->zoom = 1.0f;

    // Create MuPDF context
    renderer->ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!renderer->ctx) {
        fprintf(stderr, "Cannot create MuPDF context\n");
        free(renderer);
        return NULL;
    }

    // Register default file types
    fz_try(renderer->ctx) {
        fz_register_document_handlers(renderer->ctx);
    } fz_catch(renderer->ctx) {
        fprintf(stderr, "Cannot register document handlers: %s\n",
                fz_caught_message(renderer->ctx));
        fz_drop_context(renderer->ctx);
        free(renderer);
        return NULL;
    }

    // Open document
    fz_try(renderer->ctx) {
        renderer->doc = fz_open_document(renderer->ctx, filename);
        if (!renderer->doc) {
            fprintf(stderr, "Cannot open document: %s\n", filename);
            fz_drop_context(renderer->ctx);
            free(renderer);
            return NULL;
        }
    } fz_catch(renderer->ctx) {
        fprintf(stderr, "Cannot open document '%s': %s\n",
                filename, fz_caught_message(renderer->ctx));
        fz_drop_context(renderer->ctx);
        free(renderer);
        return NULL;
    }

    // Get page count
    fz_try(renderer->ctx) {
        renderer->page_count = fz_count_pages(renderer->ctx, renderer->doc);
        if (renderer->page_count <= 0) {
            fprintf(stderr, "Document has no pages\n");
            fz_drop_document(renderer->ctx, renderer->doc);
            fz_drop_context(renderer->ctx);
            free(renderer);

            return NULL;
        }
    } fz_catch(renderer->ctx) {
        fprintf(stderr, "Cannot count pages: %s\n", fz_caught_message(renderer->ctx));
        fz_drop_document(renderer->ctx, renderer->doc);
        fz_drop_context(renderer->ctx);
        free(renderer);

        return NULL;
    }

    printf("Successfully opened PDF: %d pages\n", renderer->page_count);
    return renderer;
}

void PDFRendererFree(PDFRenderer *renderer) {
    if (!renderer) return;

    if (renderer->doc) {
        fz_drop_document(renderer->ctx, renderer->doc);
    }

    if (renderer->ctx) {
        fz_drop_context(renderer->ctx);
    }

    free(renderer);
}

int PDFRendererGetPageCount(PDFRenderer *renderer) {
    if (!renderer) return 0;
    return renderer->page_count;
}

void PDFRendererSetZoom(PDFRenderer *renderer, float zoom) {
    if (!renderer) return;

    // Clamp zoom to reasonable values
    if (zoom < 0.1f) zoom = 0.1f;
    if (zoom > 20.0f) zoom = 20.0f;

    renderer->zoom = zoom;
}

fz_rect PDFRendererGetPageSize(PDFRenderer *renderer, int page_num) {
    fz_rect bounds = {0, 0, 0, 0};

    if (!renderer || page_num < 0 || page_num >= renderer->page_count) {
        return bounds;
    }

    fz_try(renderer->ctx) {
        fz_page *page = fz_load_page(renderer->ctx, renderer->doc, page_num);
        bounds = fz_bound_page(renderer->ctx, page);
        fz_drop_page(renderer->ctx, page);
    } fz_catch(renderer->ctx) {
        fprintf(stderr, "Cannot get page size for page %d: %s\n",
                page_num, fz_caught_message(renderer->ctx));
        bounds = (fz_rect){0, 0, 0, 0};
    }

    return bounds;
}

unsigned char* PDFRendererRenderPage(PDFRenderer *renderer, int page_num,
                                       int *width, int *height, int *stride) {
    if (!renderer || !width || !height || !stride) {
        return NULL;
    }

    if (page_num < 0 || page_num >= renderer->page_count) {
        fprintf(stderr, "Page number %d out of range (0-%d)\n",
                page_num, renderer->page_count - 1);
        return NULL;
    }

    fz_page *page = NULL;
    fz_pixmap *pix = NULL;
    unsigned char *data = NULL;

    fz_try(renderer->ctx) {
        page = fz_load_page(renderer->ctx, renderer->doc, page_num);

        fz_rect bounds = fz_bound_page(renderer->ctx, page);
        fz_matrix transform = fz_scale(renderer->zoom, renderer->zoom);
        bounds = fz_transform_rect(bounds, transform);

        // Create pixmap with RGB format
        fz_irect bbox = fz_round_rect(bounds);
        pix = fz_new_pixmap_with_bbox(renderer->ctx, fz_device_rgb(renderer->ctx),
                                      bbox, NULL, 0);

        if (!pix) {
            fprintf(stderr, "Cannot create pixmap for page %d\n", page_num);
            fz_drop_page(renderer->ctx, page);
            return NULL;
        }

        fz_clear_pixmap_with_value(renderer->ctx, pix, 0xff);
        fz_device *dev = fz_new_draw_device(renderer->ctx, transform, pix);
        fz_run_page(renderer->ctx, page, dev, fz_identity, NULL);
        fz_close_device(renderer->ctx, dev);
        fz_drop_device(renderer->ctx, dev);

        // Get pixmap info
        *width = fz_pixmap_width(renderer->ctx, pix);
        *height = fz_pixmap_height(renderer->ctx, pix);
        *stride = fz_pixmap_stride(renderer->ctx, pix);
        int components = fz_pixmap_components(renderer->ctx, pix);

        // Verify we have RGB format (3 components)
        if (components != 3) {
            fprintf(stderr, "Unexpected pixel format: %d components\n", components);
            fz_drop_pixmap(renderer->ctx, pix);
            fz_drop_page(renderer->ctx, page);

            return NULL;
        }

        // Copy pixel data
        int data_size = *height * *stride;
        data = malloc(data_size);
        if (!data) {
            fprintf(stderr, "Cannot allocate memory for pixel data\n");
            fz_drop_pixmap(renderer->ctx, pix);
            fz_drop_page(renderer->ctx, page);
            return NULL;
        }

        unsigned char *samples = fz_pixmap_samples(renderer->ctx, pix);
        memcpy(data, samples, data_size);

    } fz_catch(renderer->ctx) {
        fprintf(stderr, "Cannot render page %d: %s\n",
                page_num, fz_caught_message(renderer->ctx));
        if (data) {
            free(data);
            data = NULL;
        }
    }

    // Cleanup
    if (pix) fz_drop_pixmap(renderer->ctx, pix);
    if (page) fz_drop_page(renderer->ctx, page);

    return data;
}

float pdf_renderer_get_zoom(PDFRenderer *renderer) {
    if (!renderer) return 1.0f;
    return renderer->zoom;
}

bool pdf_renderer_is_valid_page(PDFRenderer *renderer, int page_num) {
    if (!renderer) return false;
    return (page_num >= 0 && page_num < renderer->page_count);
}

// Render page to specific format
unsigned char* pdf_renderer_render_page_format(PDFRenderer *renderer, int page_num,
                                              int target_width, int target_height,
                                              int *actual_width, int *actual_height,
                                              int *stride) {
    if (!renderer || target_width <= 0 || target_height <= 0) return NULL;

    // Calculate zoom to fit target dimensions
    fz_rect page_bounds = PDFRendererGetPageSize(renderer, page_num);
    if (page_bounds.x1 <= 0 || page_bounds.y1 <= 0) return NULL;

    float scale_x = (float)target_width / page_bounds.x1;
    float scale_y = (float)target_height / page_bounds.y1;
    float scale = fminf(scale_x, scale_y);  // Use smaller scale to fit both dimensions

    // Temporarily set zoom
    float old_zoom = renderer->zoom;
    PDFRendererSetZoom(renderer, scale);

    // Render page
    unsigned char *data = PDFRendererRenderPage(renderer, page_num,
                                                  actual_width, actual_height, stride);

    // Restore old zoom
    PDFRendererSetZoom(renderer, old_zoom);

    return data;
}