#ifndef ROVIEW_RENDERER_H
#define ROVIEW_RENDERER_H

#include <mupdf/fitz.h>

typedef struct {
    fz_context *ctx;
    fz_document *doc;
    int page_count;
    float zoom;
} PDFRenderer;

// Function declarations
PDFRenderer* PDFRendererNew(const char *filename);
void PDFRendererFree(PDFRenderer *renderer);
int PDFRendererGetPageCount(PDFRenderer *renderer);
void PDFRendererSetZoom(PDFRenderer *renderer, float zoom);
fz_rect PDFRendererGetPageSize(PDFRenderer *renderer, int page_num);
unsigned char* PDFRendererRenderPage(PDFRenderer *renderer, int page_num,
                                       int *width, int *height, int *stride);

#endif // ROVIEW_RENDERER_H