#ifndef ROVIEW_PDFPAGE_H
#define ROVIEW_PDFPAGE_H

#include <raylib.h>

typedef struct {
    Texture2D texture;
    int width;
    int height;
    int index;
} PDFPage;

#endif // ROVIEW_PDFPAGE_H