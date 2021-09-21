/*
 * Nuklear - 1.32.0 - public domain
 * no warranty implied; use at your own risk.
 * based on allegro5 version authored from 2015-2016 by Micha Mettke
 * quickdraw version camhenlin 2021
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

int widthFor12ptFont[128] = {
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

// note: if this produces a greater value than the actual length of the text, 
// the cursor will walk off to the right
// too small, it will precede the end of the text
// TODO: fully convert
// TODO: assuming system font for v1, support other fonts in v2
// doing this in a "fast" way by using a precomputed table for a 12pt font
static int nk_quickdraw_font_get_text_width(nk_handle handle, int height, const char *text, int len) {

    // writeSerialPortDebug(boutRefNum, "nk_quickdraw_font_get_text_width");

    if (!text || len == 0) {

        return 0;
    }

    int width = 0;

    for (int i = 0; i < len; i++) {

        width += widthFor12ptFont[(int)text[i]];
    }

    return width;
}

static int _get_text_width(const char *text, int len) {

    // writeSerialPortDebug(boutRefNum, "nk_quickdraw_font_get_text_width");

    if (!text || len == 0) {

        return 0;
    }

    int width = 0;

    for (int i = 0; i < len; i++) {

        width += widthFor12ptFont[(int)text[i]];
    }

    return width;
}



static int nk_color_to_quickdraw_bw_color(struct nk_color color) {

    // TODO: since we are operating under a b&w display - we need to convert these colors to black and white
    // look up a simple algorithm for taking RGBA values and making the call on black or white and try it out here
    // as a future upgrade, we could support color quickdraw
    // using an algorithm from https://stackoverflow.com/questions/3942878/how-to-decide-font-color-in-white-or-black-depending-on-background-color
    // if (red*0.299 + green*0.587 + blue*0.114) > 186 use #000000 else use #ffffff
    // return al_map_rgba((unsigned char)color.r, (unsigned char)color.g, (unsigned char)color.b, (unsigned char)color.a);
   
    float magicColorNumber = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
   
    #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

       char stringMagicColorNumber[255];
       sprintf(stringMagicColorNumber, "stringMagicColorNumber: %f", magicColorNumber);
       writeSerialPortDebug(boutRefNum, stringMagicColorNumber);
    #endif
   
   if (magicColorNumber > 37) {
       
       return blackColor;
   }
   
   return blackColor;//whiteColor;
}

// i split this in to a 2nd routine because we can use the various shades of gray when filling rectangles and whatnot
static Pattern nk_color_to_quickdraw_color(struct nk_color color) {

    // as a future upgrade, we could support color quickdraw
    // using an algorithm from https://stackoverflow.com/questions/3942878/how-to-decide-font-color-in-white-or-black-depending-on-background-color
    // if (red*0.299 + green*0.587 + blue*0.114) > 186 use #000000 else use #ffffff
    uint8_t red;
    uint8_t blue;
    uint8_t green;
    
    
    float magicColorNumber = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;

    if (magicColorNumber > 150) {

        return qd.black;
    } else if (magicColorNumber > 100) {

        return qd.dkGray;
    } else if (magicColorNumber > 75) {

        return qd.gray;
    } else if (magicColorNumber > 49) {

        return qd.ltGray;
    }

    return qd.white;
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

NK_API void nk_quickdraw_render(WindowPtr window, struct nk_context *ctx) {

    long start;
    long end;
    long total;
    start = TickCount();

    long renderTime1;
    long renderTime2;
    long renderTime3;


    void *cmds = nk_buffer_memory(&ctx->memory);

    // do not render if the buffer did not change from the previous rendering run
    if (!memcmp(cmds, last, ctx->memory.allocated)) {

        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

            writeSerialPortDebug(boutRefNum, "NK_COMMAND_NOP");
        #endif

        return;
    }

    memcpy(last, cmds, ctx->memory.allocated);

    const struct nk_command *cmd = 0;

    if (!lastEventWasKey) {
        OpenPort(&gMainOffScreen.BWPort);
        SetPort(&gMainOffScreen.BWPort);
        SetPortBits(&gMainOffScreen.BWBits);
        // EraseRect(&gMainOffScreen.bounds);
    } else {

        SetPort(window);
    }

    end = TickCount();
    total = end - start;
    renderTime1 = total;// / 60.0;
    start = TickCount();

    nk_foreach(cmd, ctx) {

        int color;
        ForeColor(blackColor);

        if (lastEventWasKey && cmd->type == NK_COMMAND_TEXT) {


                //writeSerialPortDebug(boutRefNum, "FAST INPUT");


            const struct nk_command_text *t = (const struct nk_command_text*)cmd;
            
            #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                writeSerialPortDebug(boutRefNum, "NK_COMMAND_TEXT");
                char log[255];
                sprintf(log, "%f: %c, %d", (int)t->height, &t->string, (int)t->length);
                writeSerialPortDebug(boutRefNum, log);
            #endif

            MoveTo((int)t->x + _get_text_width(&t->string, (int)t->length - 1), (int)t->y + (int)t->height);

            // DrawText((const char*)t->string, 0, (int)t->length);

            DrawChar(t->string[t->length - 1]);

        } else if (!lastEventWasKey) {


                // writeSerialPortDebug(boutRefNum, "SLOW INPUT");
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

                        const struct nk_command_scissor *s =(const struct nk_command_scissor*)cmd;
                       //  // al_set_clipping_rectangle((int)s->x, (int)s->y, (int)s->w, (int)s->h); // TODO: https://www.allegro.cc/manual/5/al_set_clipping_rectangle
                       //  // this essentially just sets the region of the screen that we are going to write to
                       //  // initially, i thought that this would be SetClip, but now believe this should be ClipRect, see:
                       //  // Inside Macintosh: Imaging with Quickdraw pages 2-48 and 2-49 for more info
                       //  // additionally, see page 2-61 for a data structure example for the rectangle OR 
                       //  // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html
                       //  // for usage example
                       // Rect quickDrawRectangle;
                       // quickDrawRectangle.top = (int)s->y;
                       // quickDrawRectangle.left = (int)s->x;
                       // quickDrawRectangle.bottom = (int)s->y + (int)s->h;
                       // quickDrawRectangle.right = (int)s->x + (int)s->w;
                       
                       // ClipRect(&quickDrawRectangle);
                    }

                    break;
                case NK_COMMAND_LINE: {
                    
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_LINE");
                        #endif

                        const struct nk_command_line *l = (const struct nk_command_line *)cmd;

                        color = nk_color_to_quickdraw_bw_color(l->color);
                        // great reference: http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html
                        // al_draw_line((float)l->begin.x, (float)l->begin.y, (float)l->end.x, (float)l->end.y, color, (float)l->line_thickness); // TODO: look up and convert al_draw_line
                        ForeColor(color);
                        PenSize((int)l->line_thickness, (int)l->line_thickness);
                        MoveTo((int)l->begin.x, (int)l->begin.y);
                        LineTo((int)l->end.x, (int)l->end.y);
                    }

                    break;
                case NK_COMMAND_RECT: {
                    
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_RECT");
                        #endif

                        // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-102.html#MARKER-9-372
                        // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-103.html#HEADING103-0
                        const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;

                        color = nk_color_to_quickdraw_bw_color(r->color);
                        
                        ForeColor(color);
                        PenSize((int)r->line_thickness, (int)r->line_thickness);

                        Rect quickDrawRectangle;
                        quickDrawRectangle.top = (int)r->y;
                        quickDrawRectangle.left = (int)r->x;
                        quickDrawRectangle.bottom = (int)r->y + (int)r->h;
                        quickDrawRectangle.right = (int)r->x + (int)r->w;

                        FrameRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding);
                    }

                    break;
                case NK_COMMAND_RECT_FILLED: {
                    
                    
                    #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                        writeSerialPortDebug(boutRefNum, "NK_COMMAND_RECT_FILLED");
                    #endif

                    const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;

                    color = nk_color_to_quickdraw_bw_color(r->color);
                    
                    ForeColor(color);
                    Pattern colorPattern = nk_color_to_quickdraw_color(r->color);

                    // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                    PenSize(1.0, 1.0); // no member line thickness on this struct so assume we want a thin line
                    // might actually need to build this with SetRect, search inside macintosh: imaging with quickdraw
                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)r->y;
                    quickDrawRectangle.left = (int)r->x;
                    quickDrawRectangle.bottom = (int)r->y + (int)r->h;
                    quickDrawRectangle.right = (int)r->x + (int)r->w;

                    FillRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding, &colorPattern);
                    FrameRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding); // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-105.html#HEADING105-0
                }

                    break;
                case NK_COMMAND_CIRCLE: {
                    
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_CIRCLE");
                        #endif

                        const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
                        
                        color = nk_color_to_quickdraw_bw_color(c->color);
                        
                        ForeColor(color);  
                        Rect quickDrawRectangle;
                        quickDrawRectangle.top = (int)c->y;
                        quickDrawRectangle.left = (int)c->x;
                        quickDrawRectangle.bottom = (int)c->y + (int)c->h;
                        quickDrawRectangle.right = (int)c->x + (int)c->w;

                        FrameOval(&quickDrawRectangle); // An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
                    }

                    break;
                case NK_COMMAND_CIRCLE_FILLED: {
                    
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_CIRCLE_FILLED");
                        #endif

                        const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
                        
                        color = nk_color_to_quickdraw_bw_color(c->color);
                        
                        ForeColor(color);
                        Pattern colorPattern = nk_color_to_quickdraw_color(c->color);
                        // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                        PenSize(1.0, 1.0);
                        Rect quickDrawRectangle;
                        quickDrawRectangle.top = (int)c->y;
                        quickDrawRectangle.left = (int)c->x;
                        quickDrawRectangle.bottom = (int)c->y + (int)c->h;
                        quickDrawRectangle.right = (int)c->x + (int)c->w;

                        FillOval(&quickDrawRectangle, &colorPattern); 
                        FrameOval(&quickDrawRectangle);// An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
                        // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-111.html#HEADING111-0
                    }

                    break;
                case NK_COMMAND_TRIANGLE: {
                    
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_TRIANGLE");
                        #endif

                        const struct nk_command_triangle *t = (const struct nk_command_triangle*)cmd;
                        color = nk_color_to_quickdraw_bw_color(t->color);
                        
                        ForeColor(color);
                        PenSize((int)t->line_thickness, (int)t->line_thickness);

                        MoveTo((int)t->a.x, (int)t->a.y);
                        LineTo((int)t->b.x, (int)t->b.y);
                        LineTo((int)t->c.x, (int)t->c.y);
                        LineTo((int)t->a.x, (int)t->a.y);
                    }

                    break;
                case NK_COMMAND_TRIANGLE_FILLED: {
                    
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_TRIANGLE_FILLED");
                        #endif

                        const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
                        Pattern colorPattern = nk_color_to_quickdraw_color(t->color);
                        color = nk_color_to_quickdraw_bw_color(t->color);
                        PenSize(1.0, 1.0);
                        // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                        ForeColor(color);

                        PolyHandle trianglePolygon = OpenPoly(); 
                        MoveTo((int)t->a.x, (int)t->a.y);
                        LineTo((int)t->b.x, (int)t->b.y);
                        LineTo((int)t->c.x, (int)t->c.y);
                        LineTo((int)t->a.x, (int)t->a.y);
                        ClosePoly();

                        FillPoly(trianglePolygon, &colorPattern);
                        KillPoly(trianglePolygon);
                    }

                    break;
                case NK_COMMAND_POLYGON: {
                    
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_POLYGON");
                        #endif

                        const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                        color = nk_color_to_quickdraw_bw_color(p->color);
                        ForeColor(color);
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

                        const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                        Pattern colorPattern = nk_color_to_quickdraw_color(p->color);
                        color = nk_color_to_quickdraw_bw_color(p->color);
                        // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48 -- but might actually need PenPat -- look into this
                        ForeColor(color);
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

                        FillPoly(trianglePolygon, &colorPattern);
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

                        color = nk_color_to_quickdraw_bw_color(p->color);
                        ForeColor(color);
                        int i;

                        for (i = 0; i < p->point_count; i++) {
                            
                            if (i == 0) {
                                
                                MoveTo(p->points[i].x, p->points[i].y);
                            }
                            
                            LineTo(p->points[i].x, p->points[i].y);
                        }
                    }

                    break;
                case NK_COMMAND_TEXT: {

                        const struct nk_command_text *t = (const struct nk_command_text*)cmd;
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_TEXT");
                            char log[255];
                            sprintf(log, "%f: %c, %d", (int)t->height, &t->string, (int)t->length);
                            writeSerialPortDebug(boutRefNum, log);
                        #endif

                        color = nk_color_to_quickdraw_bw_color(t->foreground);
                        ForeColor(color);
                        MoveTo((int)t->x, (int)t->y + (int)t->height);

                        DrawText((const char*)t->string, 0, (int)t->length);

                        // DrawChar(t->string[t->length - 1]);
                    }

                    break;
                case NK_COMMAND_CURVE: {
                        
                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_CURVE");
                        #endif

                        const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
                        color = nk_color_to_quickdraw_bw_color(q->color);
                        ForeColor(color);
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
                        
                        color = nk_color_to_quickdraw_bw_color(a->color);
                        ForeColor(color);
                        Rect arcBoundingBoxRectangle;
                        // this is kind of silly because the cx is at the center of the arc and we need to create a rectangle around it 
                        // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html#MARKER-2-116
                        int x1 = (int)a->cx - (int)a->r;
                        int y1 = (int)a->cy - (int)a->r;
                        int x2 = (int)a->cx + (int)a->r;
                        int y2 = (int)a->cy + (int)a->r;
                        SetRect(&arcBoundingBoxRectangle, x1, y1, x2, y2);
                        // SetRect(secondRect,90,20,140,70);

                        FrameArc(&arcBoundingBoxRectangle, a->a[0], a->a[1]);
                    }

                    break;
                case NK_COMMAND_IMAGE: {

                        #ifdef NK_QUICKDRAW_GRAPHICS_DEBUGGING

                            writeSerialPortDebug(boutRefNum, "NK_COMMAND_IMAGE");  
                        #endif

                        const struct nk_command_image *i = (const struct nk_command_image *)cmd;
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
        }
    }


    end = TickCount();
    total = end - start;
    renderTime2 = total;// / 60.0;
    start = TickCount();



    if (!lastEventWasKey) {
        SetPort(window);


        // our offscreen bitmap is the same size as our port rectangle, so we
        // get away with using the portRect sizing for source and destination
        CopyBits(&gMainOffScreen.bits->portBits, &window->portBits, &window->portRect, &window->portRect, srcCopy, 0L);
    }

    end = TickCount();
    total = end - start;
    renderTime3 = total;// / 60.0;
    start = TickCount();

    
    // char logx[255];
    // sprintf(logx, "nk_quickdraw_render() renderTime1 (pre-render) %ld, renderTime2 (render loop) %ld, renderTime3 (post-render) %ld ticks to execute\n", renderTime1, renderTime2, renderTime3);
    // writeSerialPortDebug(boutRefNum, logx);

    lastEventWasKey = 0;
}

NK_API int nk_quickdraw_handle_event(EventRecord *event, struct nk_context *nuklear_context) { 
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
            writeSerialPortDebug(boutRefNum, "osEvt");

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
                        sprintf(logx, "mouse location at time of click h: %d,  v: %d", x, y);
                        writeSerialPortDebug(boutRefNum, logx);
                    #endif

                    // nk_input_motion(nuklear_context, x, y); // you can enable this if you don't want to use motion tracking
                    // in the Mac event loop handler as in the nuklear quickdraw sample, and this will allow mouse clicks to
                    // work properly, but will not allow hover states to work
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
                } else if (key == deleteKey) {
                    
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
                    
                    nk_input_unicode(nuklear_context, charKey);
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

NK_INTERN void nk_quickdraw_clipboard_copy(nk_handle usr, const char *text, int len) {

    // in Macintosh Toolbox the clipboard is referred to as "scrap manager"
    PutScrap(len, 'TEXT', text);
}

// it us up to our "main" function to call this code
NK_API struct nk_context* nk_quickdraw_init(unsigned int width, unsigned int height) {

    // needed to calculate bezier info, see mactech article.
    setupBezier();

    NewShockBitmap(&gMainOffScreen, width, height);
    NkQuickDrawFont *quickdrawfont = nk_quickdraw_font_create_from_file();
    struct nk_user_font *font = &quickdrawfont->nk;

    last = calloc(1, 64 * 1024);
    buf = calloc(1, 64 * 1024);
    nk_init_fixed(&quickdraw.nuklear_context, buf, 64 * 1024, font);
    
    // nk_init_default(&quickdraw.nuklear_context, font);
    nk_style_push_font(&quickdraw.nuklear_context, font);

    // this is pascal code but i think we would need to do something like this if we want this function 
    // to be responsible for setting the window size
    //    Region locUpdateRgn = NewRgn();
    //    SetRect(limitRect, kMinDocSize, kMinDocSize, kMaxDocSize, kMaxDocSize);
    //    // {call Window Manager to let user drag size box}
    //    growSize = GrowWindow(thisWindow, event.where, limitRect);
    //    SizeWindow(thisWindow, LoWord(growSize), HiWord(growSize), TRUE);
    //    SectRect(oldViewRect, myData^^.editRec^^.viewRect, oldViewRect);
    //    // {validate the intersection (don't update)}
    //    ValidRect(oldViewRect);
    //    // {invalidate any prior update region}
    //    InvalRgn(locUpdateRgn);
    //    DisposeRgn(locUpdateRgn);

    quickdraw.nuklear_context.clip.copy = nk_quickdraw_clipboard_copy;
    quickdraw.nuklear_context.clip.paste = nk_quickdraw_clipboard_paste;
    quickdraw.nuklear_context.clip.userdata = nk_handle_ptr(0);

    // fix styles to be more "mac-like"
    struct nk_style *style;
    struct nk_style_toggle *toggle;
    struct nk_style_button *button;
    style = &quickdraw.nuklear_context.style;

    /* checkbox toggle */
    toggle = &style->checkbox;
    nk_zero_struct(*toggle);
    toggle->normal          = nk_style_item_color(nk_rgba(45, 45, 45, 255));
    toggle->hover           = nk_style_item_color(nk_rgba(80, 80, 80, 255)); // this is the "background" hover state regardless of checked status - we want light gray
    toggle->active          = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // i can't tell what this does yet
    toggle->cursor_normal   = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the "checked" box itself - we want "black"
    toggle->cursor_hover    = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the hover state of a "checked" box - anything lighter than black is ok
    toggle->userdata        = nk_handle_ptr(0);
    toggle->text_background = nk_rgba(255, 255, 255, 255);
    toggle->text_normal     = nk_rgba(70, 70, 70, 255);
    toggle->text_hover      = nk_rgba(70, 70, 70, 255);
    toggle->text_active     = nk_rgba(70, 70, 70, 255);
    toggle->padding         = nk_vec2(3.0f, 3.0f);
    toggle->touch_padding   = nk_vec2(0,0);
    toggle->border_color    = nk_rgba(0,0,0,0);
    toggle->border          = 0.0f;
    toggle->spacing         = 5;

    /* option toggle */
    toggle = &style->option;
    nk_zero_struct(*toggle);
    toggle->normal          = nk_style_item_color(nk_rgba(45, 45, 45, 255));
    toggle->hover           = nk_style_item_color(nk_rgba(80, 80, 80, 255)); // this is the "background" hover state regardless of checked status - we want light gray
    toggle->active          = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // i can't tell what this does yet
    toggle->cursor_normal   = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the "checked" box itself - we want "black"
    toggle->cursor_hover    = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the hover state of a "checked" box - anything lighter than black is ok
    toggle->userdata        = nk_handle_ptr(0);
    toggle->text_background = nk_rgba(255, 255, 255, 255);
    toggle->text_normal     = nk_rgba(70, 70, 70, 255);
    toggle->text_hover      = nk_rgba(70, 70, 70, 255);
    toggle->text_active     = nk_rgba(70, 70, 70, 255);
    toggle->padding         = nk_vec2(3.0f, 3.0f);
    toggle->touch_padding   = nk_vec2(0,0);
    toggle->border_color    = nk_rgba(0,0,0,0);
    toggle->border          = 0.0f;
    toggle->spacing         = 5;

    // button
    button = &style->button;
    nk_zero_struct(*button);
    button->normal          = nk_style_item_color(nk_rgba(0, 0, 0, 255));
    button->hover           = nk_style_item_color(nk_rgba(80, 80, 80, 255));
    button->active          = nk_style_item_color(nk_rgba(150, 150, 150, 255));
    button->border_color    = nk_rgba(255, 255, 255, 255);
    button->text_background = nk_rgba(255, 255, 255, 255);
    button->text_normal     = nk_rgba(70, 70, 70, 255);
    button->text_hover      = nk_rgba(70, 70, 70, 255);
    button->text_active     = nk_rgba(0, 0, 0, 255);
    button->padding         = nk_vec2(2.0f,2.0f);
    button->image_padding   = nk_vec2(0.0f,0.0f);
    button->touch_padding   = nk_vec2(0.0f, 0.0f);
    button->userdata        = nk_handle_ptr(0);
    button->text_alignment  = NK_TEXT_CENTERED;
    button->border          = 1.0f;
    button->rounding        = 4.0f;
    button->draw_begin      = 0;
    button->draw_end        = 0;

    ForeColor(blackColor);

    TextSize(13);


    return &quickdraw.nuklear_context;
}

NK_API void nk_quickdraw_shutdown(void) {

    nk_free(&quickdraw.nuklear_context);
    memset(&quickdraw, 0, sizeof(quickdraw));
}
        

        
