/*
 * Nuklear - 1.32.0 - public domain
 * no warranty implied; use at your own risk.
 * based on allegro5 version authored from 2015-2016 by Micha Mettke
 * quickdraw version camhenlin 2021-2022
 * 
 * v1 intent:
 * - only default system font support
 * - no graphics/images support - quickdraw has very limited support for this
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_QUICKDRAW_H_
#define NK_QUICKDRAW_H_
#include <MacTypes.h>
#include <Types.h>
#include <Quickdraw.h>
#include <Scrap.h>
#include <Serial.h>
#include "SerialHelper.h"

#define ENABLED_DOUBLE_BUFFERING
// #define COMMAND_CACHING
// #define NK_QUICKDRAW_GRAPHICS_DEBUGGING

Boolean lastInputWasBackspace;

typedef struct NkQuickDrawFont NkQuickDrawFont;
NK_API struct nk_context* nk_quickdraw_init(unsigned int width, unsigned int height);
NK_API int nk_quickdraw_handle_event(EventRecord *event, struct nk_context *nuklear_context);
NK_API void nk_quickdraw_shutdown(void);
NK_API void nk_quickdraw_render(WindowPtr window, struct nk_context *ctx);

NK_API struct nk_image* nk_quickdraw_create_image(const char* file_name);
NK_API void nk_quickdraw_del_image(struct nk_image* image);
NK_API NkQuickDrawFont* nk_quickdraw_font_create_from_file();

#endif

/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#define MAX_MEMORY_IN_KB 6
#ifdef NK_QUICKDRAW_IMPLEMENTATION
#ifndef NK_QUICKDRAW_TEXT_MAX
#define NK_QUICKDRAW_TEXT_MAX 256
#endif
#endif
struct NkQuickDrawFont {
    struct nk_user_font nk;
    char *font;
};

int lastEventWasKey = 0;
void *last;
void *buf;

// constant keyboard mappings for convenenience
// See Inside Macintosh: Text pg A-7, A-8
int homeKey = (int)0x01;
int enterKey = (int)0x03;
int endKey = (int)0x04;
int helpKey = (int)0x05;
int backspaceKey = (int)0x08;
int deleteKey = (int)0x7F;
int tabKey = (int)0x09;
int pageUpKey = (int)0x0B;
int pageDownKey = (int)0x0C;
int returnKey = (int)0x0D;
int rightArrowKey = (int)0x1D;
int leftArrowKey = (int)0x1C;
int downArrowKey = (int)0x1F;
int upArrowKey = (int)0x1E;
int eitherShiftKey = (int)0x0F;
int escapeKey = (int)0x1B;

// #define NK_QUICKDRAW_GRAPHICS_DEBUGGING
// #def#ifdef NK_QUICKDRAW_EVENTS_DEBUGG

typedef struct {
    Ptr Address;
    long RowBytes;
    GrafPtr bits;
    Rect bounds;
    BitMap  BWBits;
    GrafPort BWPort;
    Handle  OrigBits;
} ShockBitmap;

void NewShockBitmap(ShockBitmap *theMap, short width, short height) {

    theMap->bits = 0L;
    SetRect(&theMap->bounds, 0, 0, width, height);
    
    theMap->BWBits.bounds = theMap->bounds;
    theMap->BWBits.rowBytes = ((width+15) >> 4)<<1;         // round to even
    theMap->BWBits.baseAddr = NewPtr(((long) height * (long) theMap->BWBits.rowBytes));

    theMap->BWBits.baseAddr = StripAddress(theMap->BWBits.baseAddr);
    
    OpenPort(&theMap->BWPort);
    SetPort(&theMap->BWPort);
    SetPortBits(&theMap->BWBits);

    SetRectRgn(theMap->BWPort.visRgn, theMap->bounds.left, theMap->bounds.top, theMap->bounds.right, theMap->bounds.bottom);
    SetRectRgn(theMap->BWPort.clipRgn, theMap->bounds.left, theMap->bounds.top, theMap->bounds.right, theMap->bounds.bottom);
    EraseRect(&theMap->bounds);
      
    theMap->Address = theMap->BWBits.baseAddr;
    theMap->RowBytes = (long) theMap->BWBits.rowBytes;
    theMap->bits = (GrafPtr) &theMap->BWPort;
}

ShockBitmap gMainOffScreen;

// bezier code is from http://preserve.mactech.com/articles/mactech/Vol.05/05.01/BezierCurve/index.html
// as it is not built in to quickdraw like other "modern" graphics environments
/*
   The greater the number of curve segments, the smoother the curve, 
and the longer it takes to generate and draw.  The number below was pulled 
out of a hat, and seems to work o.k.
 */
#define SEGMENTS 16

static Fixed weight1[SEGMENTS + 1];
static Fixed weight2[SEGMENTS + 1];

#define w1(s) weight1[s]
#define w2(s) weight2[s]
#define w3(s) weight2[SEGMENTS - s]
#define w4(s) weight1[SEGMENTS - s]

/*
 *  SetupBezier  --  one-time setup code.
 * Compute the weights for the Bezier function.
 *  For the those concerned with space, the tables can be precomputed. 
Setup is done here for purposes of illustration.
 */
void setupBezier() {

    Fixed t, zero, one;
    int s;

    zero = FixRatio(0, 1);
    one = FixRatio(1, 1);
    weight1[0] = one;
    weight2[0] = zero;

    for (s = 1; s < SEGMENTS; ++s) {

        t = FixRatio(s, SEGMENTS);
        weight1[s] = FixMul(one - t, FixMul(one - t, one - t));
        weight2[s] = 3 * FixMul(t, FixMul(t - one, t - one));
    }

    weight1[SEGMENTS] = zero;
    weight2[SEGMENTS] = zero;
}

/*
 *  computeSegments  --  compute segments for the Bezier curve
 * Compute the segments along the curve.
 *  The curve touches the endpoints, so don’t bother to compute them.
 */
static void computeSegments(p1, p2, p3, p4, segment) Point  p1, p2, p3, p4; Point  segment[]; {

    int s;

    segment[0] = p1;

    for (s = 1; s < SEGMENTS; ++s) {

        segment[s].v = FixRound(w1(s) * p1.v + w2(s) * p2.v + w3(s) * p3.v + w4(s) * p4.v);
        segment[s].h = FixRound(w1(s) * p1.h + w2(s) * p2.h + w3(s) * p3.h + w4(s) * p4.h);
    }

    segment[SEGMENTS] = p4;
}

/*
 *  BezierCurve  --  Draw a Bezier Curve
 * Draw a curve with the given endpoints (p1, p4), and the given 
 * control points (p2, p3).
 *  Note that we make no assumptions about pen or pen mode.
 */
void BezierCurve(p1, p2, p3, p4) Point  p1, p2, p3, p4; {

    int s;
    Point segment[SEGMENTS + 1];

    computeSegments(p1, p2, p3, p4, segment);
    MoveTo(segment[0].h, segment[0].v);

    for (s = 1 ; s <= SEGMENTS ; ++s) {

        if (segment[s].h != segment[s - 1].h || segment[s].v != segment[s - 1].v) {

            LineTo(segment[s].h, segment[s].v);
        }
    }
}

// ex usage:
// Point   control[4] = {{144,72}, {72,144}, {216,144}, {144,216}};
// BezierCurve(c[0], c[1], c[2], c[3]);

static struct nk_quickdraw {
    unsigned int width;
    unsigned int height;
    struct nk_context nuklear_context;
    struct nk_buffer cmds;
} quickdraw;

// TODO: maybe V2 - skipping images for first pass
NK_API struct nk_image* nk_quickdraw_create_image(const char* file_name) {

    // TODO: just read the file to a char, we can draw it using quickdraw
    // b&w bitmaps are pretty easy to draw...
    // for us to do this, we need to figure out the format, then figure out if we can make it in to a quickdraw rect
    // and set the buffer to the image handle pointer in the image struct
    // i assume this gets consumed elsewhere in the code, thinking NK_COMMAND_IMAGE
    char* bitmap = ""; //al_load_bitmap(file_name); // TODO: https://www.allegro.cc/manual/5/al_load_bitmap, loads file to in-memory buffer understood by allegro

    if (bitmap == NULL) {

        fprintf(stdout, "Unable to load image file: %s\n", file_name);
        return NULL;
    }

    struct nk_image *image = (struct nk_image*)calloc(1, sizeof(struct nk_image));
    image->handle.ptr = bitmap;
    image->w = 0; // al_get_bitmap_width(bitmap); // TODO: this can be retrieved from a bmp file
    image->h = 0; // al_get_bitmap_height(bitmap); // TODO: this can be retrieved from a bmp file

    return image;
}

// TODO: maybe V2 - skipping images for first pass
NK_API void nk_quickdraw_del_image(struct nk_image* image) {

    if (!image) {
        
        return;
    }

    // al_destroy_bitmap(image->handle.ptr); // TODO: https://www.allegro.cc/manual/5/al_destroy_bitmap
    //this is just de-allocating the memory from a loaded bitmap

    free(image);
}

// NOTE: you will need to generate this yourself if you want to add additional font support!
short widthFor12ptFont[128] = {
    0,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    8,
    10,
    10,
    10,
    0,
    10,
    10,
    10,
    11,
    11,
    9,
    11,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,
    4,
    6,
    7,
    10,
    7,
    11,
    10,
    3,
    5,
    5,
    7,
    7,
    4,
    7,
    4,
    7,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    4,
    4,
    6,
    8,
    6,
    8,
    11,
    8,
    8,
    8,
    8,
    7,
    7,
    8,
    8,
    6,
    7,
    9,
    7,
    12,
    9,
    8,
    8,
    8,
    8,
    7,
    6,
    8,
    8,
    12,
    8,
    8,
    8,
    5,
    7,
    5,
    8,
    8,
    6,
    8,
    8,
    7,
    8,
    8,
    6,
    8,
    8,
    4,
    6,
    8,
    4,
    12,
    8,
    8,
    8,
    8,
    6,
    7,
    6,
    8,
    8,
    12,
    8,
    8,
    8,
    5,
    5,
    5,
    8,
    8
};

// doing this in a "fast" way by using a precomputed table for a 12pt font
static short nk_quickdraw_font_get_text_width(nk_handle handle, short height, const char *text, short len) {

    // this is going to produce a lot of logging and not a lot of value:
    // #ifdef DEBUG_FUNCTION_CALLS
    //     writeSerialPortDebug(boutRefNum, "DEBUG_FUNCTION_CALLS: nk_quickdraw_font_get_text_width");
    // #endif

    if (!text || len == 0) {

        return 0;
    }

    short width = 0;

    for (short i = 0; i < len; i++) {

        width += widthFor12ptFont[(int)text[i]];
    }

    return width;
}

static int _get_text_width(const char *text, int len) {

    #ifdef DEBUG_FUNCTION_CALLS
        writeSerialPortDebug(boutRefNum, "DEBUG_FUNCTION_CALLS: _get_text_width");
    #endif

    if (!text || len == 0) {

        return 0;
    }

    int width = 0;

    for (int i = 0; i < len; i++) {

        width += widthFor12ptFont[(int)text[i]];
    }

    return width;
}

/* Flags are identical to al_load_font() flags argument */
NK_API NkQuickDrawFont* nk_quickdraw_font_create_from_file() {

    NkQuickDrawFont *font = (NkQuickDrawFont*)calloc(1, sizeof(NkQuickDrawFont));

    font->font = calloc(1, 1024);
    if (font->font == NULL) {
        fprintf(stdout, "Unable to load font file\n");
        return NULL;
    }
    font->nk.userdata = nk_handle_ptr(font);
    font->nk.height = (int)12;
    font->nk.width = nk_quickdraw_font_get_text_width;
    return font;
}

// used for bounds checking
int mostLeft = WINDOW_WIDTH;
int mostBottom = 1;
int mostTop = WINDOW_HEIGHT;
int mostRight = 1;

void updateBounds(int top, int bottom, int left, int right) {

    #ifdef DEBUG_FUNCTION_CALLS
        writeSerialPortDebug(boutRefNum, "DEBUG_FUNCTION_CALLS: updateBounds");
    #endif

    if (left < mostLeft) {

        mostLeft = left;
    }

    if (right > mostRight) {

        mostRight = right;
    }

    if (top < mostTop) {

        mostTop = top;
    }

    if (bottom > mostBottom) {

        mostBottom = bottom;
    }
}

#ifdef COMMAND_CACHING
    void runDrawCommand(const struct nk_command *cmd, const struct nk_command *lastCmd) {
#else
    void runDrawCommand(const struct nk_command *cmd) {
#endif
    #ifdef DEBUG_FUNCTION_CALLS
        writeSerialPortDebug(boutRefNum, "DEBUG_FUNCTION_CALLS: runDrawCommand");
    #endif

    switch (cmd->type) {

        case NK_COMMAND_NOP:

            #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                writeSerialPortDebug(boutRefNum, "NK_COMMAND_NOP");
            #endif

            break;
        case NK_COMMAND_SCISSOR: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_SCISSOR");
                #endif

                const struct nk_command_scissor *s = (const struct nk_command_scissor*)cmd;

                // there is no point in supressing scissor commands because they only affect
                // where we can actually draw to:
                // #ifdef COMMAND_CACHING
                //     if (memcmp(s, lastCmd, sizeof(struct nk_command_scissor)) == 0) {

                //         #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                //             writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_scissor");
                //         #endif

                //         break;
                //     }
                // #endif

                Rect quickDrawRectangle;
                quickDrawRectangle.top = s->y;
                quickDrawRectangle.left = s->x;
                quickDrawRectangle.bottom = s->y + s->h;
                quickDrawRectangle.right = s->x + s->w;

                #ifdef ENABLED_DOUBLE_BUFFERING
                    // we use "-8192" here to filter out nuklear "nk_null_rect" which we do not want updating bounds
                    if (quickDrawRectangle.top != -8192) {

                        updateBounds(quickDrawRectangle.top, quickDrawRectangle.bottom, quickDrawRectangle.left, quickDrawRectangle.right);
                    }
                #endif

                ClipRect(&quickDrawRectangle);
            }

            break;
        case NK_COMMAND_RECT: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_RECT");
                #endif

                // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-102.html#MARKER-9-372
                // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-103.html#HEADING103-0
                const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;

                #ifdef COMMAND_CACHING

                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(r, lastCmd, sizeof(struct nk_command_rect)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_rect");
                        #endif

                        break;
                    }
                #endif

                ForeColor(r->color);
                PenSize(r->line_thickness, r->line_thickness);

                Rect quickDrawRectangle;
                quickDrawRectangle.top = r->y;
                quickDrawRectangle.left = r->x;
                quickDrawRectangle.bottom = r->y + r->h;
                quickDrawRectangle.right = r->x + r->w;

                #ifdef ENABLED_DOUBLE_BUFFERING
                    updateBounds(quickDrawRectangle.top, quickDrawRectangle.bottom, quickDrawRectangle.left, quickDrawRectangle.right);
                #endif

                FrameRoundRect(&quickDrawRectangle, r->rounding, r->rounding);
            }

            break;
        case NK_COMMAND_RECT_FILLED: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_RECT_FILLED");
                #endif

                const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;

                if (r->allowCache == false) {

                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = r->y;
                    quickDrawRectangle.left = r->x;
                    quickDrawRectangle.bottom = r->y + r->h;
                    quickDrawRectangle.right = r->x + r->w;

                    #ifdef ENABLED_DOUBLE_BUFFERING
                        updateBounds(quickDrawRectangle.top, quickDrawRectangle.bottom, quickDrawRectangle.left, quickDrawRectangle.right);
                    #endif

                    FillRoundRect(&quickDrawRectangle, r->rounding, r->rounding, &qd.white);
                    break;
                }

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(r, lastCmd, sizeof(struct nk_command_rect_filled)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_rect_filled");
                        #endif

                        break;
                    }
                #endif

                // TODO: to support coloring the lines, we need to map from qd Pattern types to integer colors
                // color = nk_color_to_quickdraw_bw_color(r->color);
                // ForeColor(color);
                ForeColor(blackColor);

                PenSize(1.0, 1.0);

                Rect quickDrawRectangle;
                quickDrawRectangle.top = r->y;
                quickDrawRectangle.left = r->x;
                quickDrawRectangle.bottom = r->y + r->h;
                quickDrawRectangle.right = r->x + r->w;

                #ifdef ENABLED_DOUBLE_BUFFERING
                    updateBounds(quickDrawRectangle.top, quickDrawRectangle.bottom, quickDrawRectangle.left, quickDrawRectangle.right);
                #endif

                FillRoundRect(&quickDrawRectangle, r->rounding, r->rounding, &r->color);
                FrameRoundRect(&quickDrawRectangle, r->rounding, r->rounding); // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-105.html#HEADING105-0
            }

            break;
        case NK_COMMAND_TEXT: {

                const struct nk_command_text *t = (const struct nk_command_text*)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && t->allowCache && cmd->type == lastCmd->type && memcmp(t, lastCmd, sizeof(struct nk_command_text)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            char log[255];

                            sprintf(log, "ALREADY DREW CMD nk_command_text string: \"%s\", height: %d, length: %d, x: %d, y: %d, allowCache: %d", t->string, t->height, t->length, t->x, t->y, t->allowCache);
                            writeSerialPortDebug(boutRefNum, log);
                        #endif

                        break;
                    }
                #endif

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    char log[255];

                    sprintf(log, "NK_COMMAND_TEXT string: \"%s\", height: %d, length: %d, x: %d, y: %d, allowCache: %d", t->string, t->height, t->length, t->x, t->y, t->allowCache);
                    writeSerialPortDebug(boutRefNum, log);
                #endif

                Rect quickDrawRectangle;
                quickDrawRectangle.top = t->y;
                quickDrawRectangle.left = t->x;
                quickDrawRectangle.bottom = t->y + 15;
                quickDrawRectangle.right = t->x + _get_text_width((const char*)t->string, (int)t->length);

                #ifdef ENABLED_DOUBLE_BUFFERING
                    updateBounds(quickDrawRectangle.top, quickDrawRectangle.bottom, quickDrawRectangle.left, quickDrawRectangle.right);
                #endif

                #ifdef COMMAND_CACHING
                    EraseRect(&quickDrawRectangle);
                #endif

                ForeColor(t->foreground);
                MoveTo(t->x, t->y + t->height);

                PenSize(1.0, 1.0);
                DrawText((const char*)t->string, 0, t->length);
            }

            break;
        case NK_COMMAND_LINE: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_LINE");
                #endif

                const struct nk_command_line *l = (const struct nk_command_line *)cmd;

                #ifdef COMMAND_CACHING

                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(l, lastCmd, sizeof(struct nk_command_line)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_line");
                        #endif

                        break;
                    }
                #endif

                // great reference: http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html
                ForeColor(l->color);
                PenSize(l->line_thickness, l->line_thickness);
                MoveTo(l->begin.x, l->begin.y);
                LineTo(l->end.x, l->end.y);
            }

            break;
        case NK_COMMAND_CIRCLE: {
            
                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_CIRCLE");
                #endif

                const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(c, lastCmd, sizeof(struct nk_command_circle)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_circle");
                        #endif

                        break;
                    }
                #endif

                ForeColor(c->color);  
                Rect quickDrawRectangle;
                quickDrawRectangle.top = c->y;
                quickDrawRectangle.left = c->x;
                quickDrawRectangle.bottom = c->y + c->h;
                quickDrawRectangle.right = c->x + c->w;

                #ifdef ENABLED_DOUBLE_BUFFERING
                    updateBounds(quickDrawRectangle.top, quickDrawRectangle.bottom, quickDrawRectangle.left, quickDrawRectangle.right);
                #endif

                FrameOval(&quickDrawRectangle); // An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
            }

            break;
        case NK_COMMAND_CIRCLE_FILLED: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_CIRCLE_FILLED");
                #endif

                const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(c, lastCmd, sizeof(struct nk_command_circle_filled)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_circle_filled");
                        #endif

                        break;
                    }
                #endif

                ForeColor(blackColor);
                // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                PenSize(1.0, 1.0);
                Rect quickDrawRectangle;
                quickDrawRectangle.top = c->y;
                quickDrawRectangle.left = c->x;
                quickDrawRectangle.bottom = c->y + c->h;
                quickDrawRectangle.right = c->x + c->w;

                #ifdef ENABLED_DOUBLE_BUFFERING
                    updateBounds(quickDrawRectangle.top, quickDrawRectangle.bottom, quickDrawRectangle.left, quickDrawRectangle.right);
                #endif

                FillOval(&quickDrawRectangle, &c->color); 
                FrameOval(&quickDrawRectangle);// An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
                // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-111.html#HEADING111-0
            }

            break;
        case NK_COMMAND_TRIANGLE: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_TRIANGLE");
                #endif

                const struct nk_command_triangle *t = (const struct nk_command_triangle*)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(t, lastCmd, sizeof(struct nk_command_triangle)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_triangle");
                        #endif

                        break;
                    }
                #endif
                
                ForeColor(t->color);
                PenSize(t->line_thickness, t->line_thickness);

                MoveTo(t->a.x, t->a.y);
                LineTo(t->b.x, t->b.y);
                LineTo(t->c.x, t->c.y);
                LineTo(t->a.x, t->a.y);
            }

            break;
        case NK_COMMAND_TRIANGLE_FILLED: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_TRIANGLE_FILLED");
                #endif

                const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(t, lastCmd, sizeof(struct nk_command_triangle_filled)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_triangle_filled");
                        #endif

                        break;
                    }
                #endif

                PenSize(1.0, 1.0);
                // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                ForeColor(blackColor);

                PolyHandle trianglePolygon = OpenPoly(); 
                MoveTo(t->a.x, t->a.y);
                LineTo(t->b.x, t->b.y);
                LineTo(t->c.x, t->c.y);
                LineTo(t->a.x, t->a.y);
                ClosePoly();

                FillPoly(trianglePolygon, &t->color);
                KillPoly(trianglePolygon);
            }

            break;
        case NK_COMMAND_POLYGON: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_POLYGON");
                #endif

                const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(p, lastCmd, sizeof(struct nk_command_polygon)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_polygon");
                        #endif

                        break;
                    }
                #endif

                ForeColor(p->color);
                int i;

                for (i = 0; i < p->point_count; i++) {

                    if (i == 0) {

                        MoveTo(p->points[i].x, p->points[i].y);
                    }

                    LineTo(p->points[i].x, p->points[i].y);

                    if (i == p->point_count - 1) {

                        LineTo(p->points[0].x, p->points[0].y);
                    }
                }
            }
            
            break;
        case NK_COMMAND_POLYGON_FILLED: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_POLYGON_FILLED");
                #endif

                const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled*)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(p, lastCmd, sizeof(struct nk_command_polygon_filled)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_polygon_filled");
                        #endif

                        break;
                    }
                #endif

                // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48 -- but might actually need PenPat -- look into this
                ForeColor(blackColor);
                int i;

                PolyHandle trianglePolygon = OpenPoly(); 
                for (i = 0; i < p->point_count; i++) {

                    if (i == 0) {

                        MoveTo(p->points[i].x, p->points[i].y);
                    }

                    LineTo(p->points[i].x, p->points[i].y);

                    if (i == p->point_count - 1) {

                        LineTo(p->points[0].x, p->points[0].y);
                    }
                }
                
                ClosePoly();

                FillPoly(trianglePolygon, &p->color);
                KillPoly(trianglePolygon);
            }
            
            break;
        case NK_COMMAND_POLYLINE: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_POLYLINE");
                #endif

                // this is similar to polygons except the polygon does not get closed to the 0th point
                // check out the slight difference in the for loop
                const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                #ifdef COMMAND_CACHING
                    if (!forceRedraw && cmd->type == lastCmd->type && memcmp(p, lastCmd, sizeof(struct nk_command_polygon)) == 0) {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "ALREADY DREW CMD nk_command_polygon");
                        #endif

                        break;
                    }
                #endif

                ForeColor(p->color);
                int i;

                for (i = 0; i < p->point_count; i++) {

                    if (i == 0) {

                        MoveTo(p->points[i].x, p->points[i].y);
                    }
                    
                    LineTo(p->points[i].x, p->points[i].y);
                }
            }

            break;
        case NK_COMMAND_CURVE: {
                
                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_CURVE");
                #endif

                const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;

                ForeColor(q->color);
                Point p1 = { (int)q->begin.x, (int)q->begin.y};
                Point p2 = { (int)q->ctrl[0].x, (int)q->ctrl[0].y};
                Point p3 = { (int)q->ctrl[1].x, (int)q->ctrl[1].y};
                Point p4 = { (int)q->end.x, (int)q->end.y};

                BezierCurve(p1, p2, p3, p4);
            }

            break;
        case NK_COMMAND_ARC: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_ARC");
                #endif

                const struct nk_command_arc *a = (const struct nk_command_arc *)cmd;

                ForeColor(a->color);
                Rect arcBoundingBoxRectangle;
                // this is kind of silly because the cx is at the center of the arc and we need to create a rectangle around it 
                // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html#MARKER-2-116
                int x1 = (int)a->cx - (int)a->r;
                int y1 = (int)a->cy - (int)a->r;
                int x2 = (int)a->cx + (int)a->r;
                int y2 = (int)a->cy + (int)a->r;
                SetRect(&arcBoundingBoxRectangle, x1, y1, x2, y2);

                FrameArc(&arcBoundingBoxRectangle, a->a[0], a->a[1]);
            }

            break;
        case NK_COMMAND_IMAGE: {

                #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "NK_COMMAND_IMAGE");  
                #endif

                // const struct nk_command_image *i = (const struct nk_command_image *)cmd;
                // al_draw_bitmap_region(i->img.handle.ptr, 0, 0, i->w, i->h, i->x, i->y, 0); // TODO: look up and convert al_draw_bitmap_region
                // TODO: consider implementing a bitmap drawing routine. we could iterate pixel by pixel and draw
                // here is some super naive code that could work, used for another project that i was working on with a custom format but would be
                // easy to modify for standard bitmap files (just need to know how many bytes represent each pixel and iterate from there):
                // 
                // for (int i = 0; i < strlen(string); i++) {
                //     printf("\nchar: %c", string[i]);
                //     char pixel[1];
                //     memcpy(pixel, &string[i], 1);
                //     if (strcmp(pixel, "0") == 0) { // white pixel
                //         MoveTo(++x, y);
                //      } else if (strcmp(pixel, "1") == 0) { // black pixel
                //          // advance the pen and draw a 1px x 1px "line"
                //          MoveTo(++x, y);
                //          LineTo(x, y);
                //      } else if (strcmp(pixel, "|") == 0) { // next line
                //          x = 1;
                //          MoveTo(x, ++y);
                //      } else if (strcmp(pixel, "/") == 0) { // end
                //      }
                //  }
            }
            
            break;
            
        // why are these cases not implemented?
        case NK_COMMAND_RECT_MULTI_COLOR:
        case NK_COMMAND_ARC_FILLED:
        default:
        
            #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                writeSerialPortDebug(boutRefNum, "NK_COMMAND_RECT_MULTI_COLOR/NK_COMMAND_ARC_FILLED/default");
            #endif
            break;
    }

    #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

        writeSerialPortDebug(boutRefNum, "NK_COMMAND_* draw complete");
    #endif
}

#ifdef COMMAND_CACHING
    int lastCalls = 0;
    int currentCalls;
#endif

NK_API void nk_quickdraw_render(WindowPtr window, struct nk_context *ctx) {

    #ifdef DEBUG_FUNCTION_CALLS
        writeSerialPortDebug(boutRefNum, "DEBUG_FUNCTION_CALLS: nk_quickdraw_render");
    #endif

    #ifdef COMMAND_CACHING
        currentCalls = 1;
    #endif

    #ifdef PROFILING
        PROFILE_START("IN nk_quickdraw_render");
    #endif

    #ifdef PROFILING
        PROFILE_START("get cmds and memcmp them");
    #endif

    void *cmds = nk_buffer_memory(&ctx->memory);

    // do not render if the buffer did not change from the previous rendering run
    if (!memcmp(cmds, last, ctx->memory.allocated)) {

        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

            writeSerialPortDebug(boutRefNum, "NO RENDER BUFFER CHANGE, ABORT");
        #endif

        return;
    }

    #ifdef PROFILING
        PROFILE_END("get cmds and memcmp them");
    #endif

    const struct nk_command *cmd = 0;

    #ifdef ENABLED_DOUBLE_BUFFERING
        OpenPort(&gMainOffScreen.BWPort);
        SetPort(&gMainOffScreen.BWPort);
        SetPortBits(&gMainOffScreen.BWBits);
    #endif

    #ifdef PROFILING
        PROFILE_START("rendering loop and switch");
    #endif

    #ifdef COMMAND_CACHING
        const struct nk_command *lastCmd;
        lastCmd = nk_ptr_add_const(struct nk_command, last, 0);
    #endif

    nk_foreach(cmd, ctx) {

        #ifdef COMMAND_CACHING
            runDrawCommand(cmd, lastCmd);
        #else
            runDrawCommand(cmd);
        #endif

        #ifdef COMMAND_CACHING
            // TODO: if this becomes worth pursuing later, it causes address errors. I suspect that the memcpy
            // command that builds up the last variable is not properly allocating memory.
            // the address error pops up on the line of the conditional itself and can sometimes take hours to trigger.
            if (currentCalls < lastCalls && lastCmd && lastCmd->next && lastCmd->next < ctx->memory.allocated) {

                lastCmd = nk_ptr_add_const(struct nk_command, last, lastCmd->next);
            }

            currentCalls++;
        #endif
    }

    #ifdef PROFILING
        PROFILE_START("memcpy commands");
    #endif

    memcpy(last, cmds, ctx->memory.allocated);

    #ifdef PROFILING
        PROFILE_END("memcpy commands");
    #endif

    #ifdef PROFILING
        PROFILE_END("rendering loop and switch");
    #endif

    #ifdef COMMAND_CACHING
        lastCalls = currentCalls;
        lastInputWasBackspace = false;
    #endif

    #ifdef ENABLED_DOUBLE_BUFFERING

        #ifdef PROFILING
            PROFILE_START("copy bits");
        #endif

        SetPort(window);

        Rect quickDrawRectangle;
        quickDrawRectangle.top = mostTop;
        quickDrawRectangle.left = mostLeft;
        quickDrawRectangle.bottom = mostBottom;
        quickDrawRectangle.right = mostRight;

        CopyBits(&gMainOffScreen.bits->portBits, &window->portBits, &quickDrawRectangle, &quickDrawRectangle, srcCopy, 0L);

        mostLeft = WINDOW_WIDTH;
        mostBottom = 1;
        mostTop = WINDOW_HEIGHT;
        mostRight = 1;

        #ifdef PROFILING
            PROFILE_END("copy bits");
        #endif
    #endif

    #ifdef PROFILING
        PROFILE_END("IN nk_quickdraw_render");
    #endif
}

NK_API int nk_quickdraw_handle_event(EventRecord *event, struct nk_context *nuklear_context) { 

    #ifdef DEBUG_FUNCTION_CALLS
        writeSerialPortDebug(boutRefNum, "DEBUG_FUNCTION_CALLS: nk_quickdraw_handle_event");
    #endif
    // see: inside macintosh: toolbox essentials 2-4
    // and  inside macintosh toolbox essentials 2-79

    WindowPtr window;
    FindWindow(event->where, &window); 
    // char logb[255];
    // sprintf(logb, "nk_quickdraw_handle_event event %d", event->what);
    // writeSerialPortDebug(boutRefNum, logb);

    switch (event->what) {
        case updateEvt: {
                return 1;
            }
            break;
        case osEvt: { 
                // the quicktime osEvts are supposed to cover mouse movement events
                // notice that we are actually calling nk_input_motion in the EventLoop for the program
                // instead, as handling this event directly does not appear to work for whatever reason
                // TODO: research this
                // writeSerialPortDebug(boutRefNum, "osEvt");

                switch (event->message) {

                    case mouseMovedMessage: {

                        #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING
                            
                            writeSerialPortDebug(boutRefNum, "mouseMovedMessage");
                        #endif


                        // event->where should have coordinates??? or is it just a pointer to what the mouse is over?
                        // TODO need to figure this out
                        nk_input_motion(nuklear_context, event->where.h, event->where.v); // TODO figure out mouse coordinates - not sure if this is right

                        break;
                    }

                    return 1;
                }
            }
            break;
        
        case mouseUp: 
            #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                writeSerialPortDebug(boutRefNum, "mouseUp!!!");
            #endif
        case mouseDown: {

            #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                writeSerialPortDebug(boutRefNum, "mouseUp/Down");
            #endif
            
            short part = FindWindow(event->where, &window);

            switch (part) {
                case inContent: {
                    // event->where should have coordinates??? or is it just a pointer to what the mouse is over?
                    // TODO need to figure this out
                    #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                        writeSerialPortDebug(boutRefNum, "mouseUp/Down IN DEFAULT ZONE!!!!");
                    #endif

                    // this converts the offset of the window to the actual location of the mouse within the window
                    Point tempPoint;
                    SetPt(&tempPoint, event->where.h, event->where.v);
                    GlobalToLocal(&tempPoint);
                    
                    if (!event->where.h) {
                        
                        #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "no event location for mouse!!!!");
                        #endif
                        return 1;
                    }
                    int x = tempPoint.h;
                    int y = tempPoint.v;

                    #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                        char logx[255];
                        sprintf(logx, "mouse location at time of click h: %d,  v: %d, is mouse down: %d", x, y, event->what == mouseDown);
                        writeSerialPortDebug(boutRefNum, logx);
                    #endif

                    // nk_input_motion(nuklear_context, x, y); // you can enable this if you don't want to use motion tracking
                    // in the Mac event loop handler as in the nuklear quickdraw sample, and this will allow mouse clicks to
                    // work properly, but will not allow hover states to work
                    nk_input_motion(nuklear_context, x, y);
                    nk_input_button(nuklear_context, NK_BUTTON_LEFT, x, y, event->what == mouseDown);
                }
                break;
                return 1;
            }
            
            break;
        case keyDown:
        case autoKey: {/* check for menukey equivalents */

                char charKey = event->message & charCodeMask;
                int key = (int)charKey;


                #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "keyDown/autoKey");

                    char logy[255];
                    sprintf(logy, "key pressed: key: '%c', 02x: '%02X', return: '%02X', %d == %d ??", key, key, returnKey, (int)(key), (int)(returnKey));
                    writeSerialPortDebug(boutRefNum, logy);
                #endif

                const Boolean isKeyDown = event->what == keyDown;

                if (event->modifiers & cmdKey) {/* Command key down */

                    if (isKeyDown) {

                        // AdjustMenus();						/* enable/disable/check menu items properly */
                        // DoMenuCommand(MenuKey(key));
                    }
                    
                    if (key == 'c') {
                        
                        nk_input_key(nuklear_context, NK_KEY_COPY, 1);
                    } else if (key == 'v') {
                        
                        nk_input_key(nuklear_context, NK_KEY_PASTE, 1);
                    } else if (key == 'x') {
                        
                        nk_input_key(nuklear_context, NK_KEY_CUT, 1);
                    } else if (key == 'z') {
                        
                        nk_input_key(nuklear_context, NK_KEY_TEXT_UNDO, 1);
                    } else if (key == 'r') {
                        
                        nk_input_key(nuklear_context, NK_KEY_TEXT_REDO, 1);
                    } 
                } else if (key == eitherShiftKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_SHIFT, isKeyDown);
                } else if (key == deleteKey && isKeyDown) {

                    nk_input_key(nuklear_context, NK_KEY_DEL, isKeyDown);
                } else if (key == enterKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_ENTER, isKeyDown);
                } else if (key == returnKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_ENTER, isKeyDown);
                } else if (key == tabKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_TAB, isKeyDown);
                } else if (key == leftArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_LEFT, isKeyDown);
                } else if (key == rightArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_RIGHT, isKeyDown);
                } else if (key == upArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_UP, isKeyDown);
                } else if (key == downArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_DOWN, isKeyDown);
                } else if (key == backspaceKey) {
                    
                    #ifdef COMMAND_CACHING
                        lastInputWasBackspace = true;
                    #endif
                    nk_input_key(nuklear_context, NK_KEY_BACKSPACE, isKeyDown);
                } else if (key == escapeKey) {
                    
                    // nk_input_key(nuklear_context, NK_KEY_TEXT_RESET_MODE, isKeyDown);
                } else if (key == pageUpKey) {
                 
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_UP, isKeyDown);
                } else if (key == pageDownKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_DOWN, isKeyDown);
                } else if (key == homeKey) {

                    // nk_input_key(nuklear_context, NK_KEY_TEXT_START, isKeyDown);
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_START, isKeyDown);
                } else if (key == endKey) {

                    // nk_input_key(nuklear_context, NK_KEY_TEXT_END, isKeyDown);
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_END, isKeyDown);
                } else {

                    #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                        writeSerialPortDebug(boutRefNum, "default keydown/autokey event");
                    #endif
                    
                    nk_input_char(nuklear_context, charKey);
                }

                lastEventWasKey = 1;

                return 1;
            }
            break;
        default: {
                #ifdef NK_QUICKDRAW_EVENTS_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "default unhandled event");
                #endif
            
                return 1; 
            }
            break;
        }
    }

    return 1;
}

// i think these functions are close to correct, but throw an error around invalid storage class
NK_INTERN void nk_quickdraw_clipboard_paste(nk_handle usr, struct nk_text_edit *edit) {    

    Handle hDest = NewHandle(0); //  { automatically resizes it}
    HLock(hDest);
    // {put data into memory referenced thru hDest handle}
    int sizeOfSurfData = GetScrap(hDest, 'TEXT', 0);
    HUnlock(hDest);
    nk_textedit_paste(edit, (char *)hDest, sizeOfSurfData);
    DisposeHandle(hDest);
}

NK_INTERN void nk_quickdraw_clipboard_copy(nk_handle usr, const char *text, short len) {

    // in Macintosh Toolbox the clipboard is referred to as "scrap manager"
    PutScrap(len, 'TEXT', text);
}

// it us up to our "main" function to call this code
NK_API struct nk_context* nk_quickdraw_init(unsigned int width, unsigned int height) {

    // needed to calculate bezier info, see mactech article.
    setupBezier();

    #ifdef ENABLED_DOUBLE_BUFFERING
        NewShockBitmap(&gMainOffScreen, width, height);
    #else
        TextFont(0);
        TextSize(12);
        TextFace(0);
    #endif

    NkQuickDrawFont *quickdrawfont = nk_quickdraw_font_create_from_file();
    struct nk_user_font *font = &quickdrawfont->nk;

    last = calloc(1, MAX_MEMORY_IN_KB * 1024);
    buf = calloc(1, MAX_MEMORY_IN_KB * 1024);
    nk_init_fixed(&quickdraw.nuklear_context, buf, MAX_MEMORY_IN_KB * 1024, font);

    nk_style_push_font(&quickdraw.nuklear_context, font);

    quickdraw.nuklear_context.clip.copy = nk_quickdraw_clipboard_copy;
    quickdraw.nuklear_context.clip.paste = nk_quickdraw_clipboard_paste;
    quickdraw.nuklear_context.clip.userdata = nk_handle_ptr(0);

    ForeColor(blackColor);

    return &quickdraw.nuklear_context;
}

NK_API void nk_quickdraw_shutdown(void) {

    nk_free(&quickdraw.nuklear_context);
    memset(&quickdraw, 0, sizeof(quickdraw));
}
        


