/* display.c — SDL2 NV12 renderer.
 *
 * On macOS we allow renderer selection/override, but keep OpenGL as the
 * default safe backend because Metal can flicker on some Tahoe-era systems.
 * SDL_PIXELFORMAT_NV12 + SDL_UpdateNVTexture lets us upload YUV planes
 * directly to GPU; the shader does YUV→RGB conversion on the GPU.
 */

#include "display.h"
#include "tb_i18n.h"
#include "tb_gesture_bridge.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include <SDL.h>
#include <dlfcn.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TB_RECEIVER_VERSION
#define TB_RECEIVER_VERSION "3.5.2"
#endif

#ifndef TB_RECEIVER_BUILD
#define TB_RECEIVER_BUILD "dev"
#endif

struct tb_display {
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_Texture  *tex;
    SDL_Texture  *status_tex;
    int           tex_w, tex_h;
    int           quit;
    int           preferred_fullscreen;
    int           is_connected;
    int           is_connecting;
    int           input_capture_active;
    int           input_intercept_active;
    struct tb_input_queue input_queue;
    uint32_t      last_target_switch_tick;
    uint32_t      last_space_switch_tick;
    uint32_t      last_space_gesture_tick;
    int           space_gesture_accum_x;
    int           cursor_x, cursor_y;
    int           cursor_source_w, cursor_source_h;
    int           cursor_visible;
    int           cursor_type;
    int           cursor_large;
    uint32_t      last_video_frame_time;
    int           system_cursor_hidden;

    char          last_ip[64];
    char          last_status[128];
    char          last_sender[128];
    char          last_panel[128];
    char          last_mode[128];
    char          last_language[96];
    char          last_permissions[160];
    char          last_address_label[96];
    int           last_drawable_w;
    int           last_drawable_h;
    int           status_is_connecting;
    CGImageRef    app_icon;
    int           app_icon_loaded;
};

static uint16_t tb_disp_mac_keycode_for_sdl_scancode(SDL_Scancode scancode) {
    switch (scancode) {
    case SDL_SCANCODE_A: return 0x00;
    case SDL_SCANCODE_S: return 0x01;
    case SDL_SCANCODE_D: return 0x02;
    case SDL_SCANCODE_F: return 0x03;
    case SDL_SCANCODE_H: return 0x04;
    case SDL_SCANCODE_G: return 0x05;
    case SDL_SCANCODE_Z: return 0x06;
    case SDL_SCANCODE_X: return 0x07;
    case SDL_SCANCODE_C: return 0x08;
    case SDL_SCANCODE_V: return 0x09;
    case SDL_SCANCODE_B: return 0x0B;
    case SDL_SCANCODE_Q: return 0x0C;
    case SDL_SCANCODE_W: return 0x0D;
    case SDL_SCANCODE_E: return 0x0E;
    case SDL_SCANCODE_R: return 0x0F;
    case SDL_SCANCODE_Y: return 0x10;
    case SDL_SCANCODE_T: return 0x11;
    case SDL_SCANCODE_1: return 0x12;
    case SDL_SCANCODE_2: return 0x13;
    case SDL_SCANCODE_3: return 0x14;
    case SDL_SCANCODE_4: return 0x15;
    case SDL_SCANCODE_6: return 0x16;
    case SDL_SCANCODE_5: return 0x17;
    case SDL_SCANCODE_EQUALS: return 0x18;
    case SDL_SCANCODE_9: return 0x19;
    case SDL_SCANCODE_7: return 0x1A;
    case SDL_SCANCODE_MINUS: return 0x1B;
    case SDL_SCANCODE_8: return 0x1C;
    case SDL_SCANCODE_0: return 0x1D;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1E;
    case SDL_SCANCODE_O: return 0x1F;
    case SDL_SCANCODE_U: return 0x20;
    case SDL_SCANCODE_LEFTBRACKET: return 0x21;
    case SDL_SCANCODE_I: return 0x22;
    case SDL_SCANCODE_P: return 0x23;
    case SDL_SCANCODE_RETURN: return 0x24;
    case SDL_SCANCODE_L: return 0x25;
    case SDL_SCANCODE_J: return 0x26;
    case SDL_SCANCODE_APOSTROPHE: return 0x27;
    case SDL_SCANCODE_K: return 0x28;
    case SDL_SCANCODE_SEMICOLON: return 0x29;
    case SDL_SCANCODE_BACKSLASH: return 0x2A;
    case SDL_SCANCODE_COMMA: return 0x2B;
    case SDL_SCANCODE_SLASH: return 0x2C;
    case SDL_SCANCODE_N: return 0x2D;
    case SDL_SCANCODE_M: return 0x2E;
    case SDL_SCANCODE_PERIOD: return 0x2F;
    case SDL_SCANCODE_TAB: return 0x30;
    case SDL_SCANCODE_SPACE: return 0x31;
    case SDL_SCANCODE_GRAVE: return 0x32;
    case SDL_SCANCODE_BACKSPACE: return 0x33;
    case SDL_SCANCODE_ESCAPE: return 0x35;
    case SDL_SCANCODE_LGUI: return 0x37;
    case SDL_SCANCODE_LSHIFT: return 0x38;
    case SDL_SCANCODE_CAPSLOCK: return 0x39;
    case SDL_SCANCODE_LALT: return 0x3A;
    case SDL_SCANCODE_LCTRL: return 0x3B;
    case SDL_SCANCODE_RSHIFT: return 0x3C;
    case SDL_SCANCODE_RALT: return 0x3D;
    case SDL_SCANCODE_RCTRL: return 0x3E;
    case SDL_SCANCODE_RGUI: return 0x36;
    case SDL_SCANCODE_F17: return 0x40;
    case SDL_SCANCODE_KP_DECIMAL: return 0x41;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x43;
    case SDL_SCANCODE_KP_PLUS: return 0x45;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x47;
    case SDL_SCANCODE_KP_DIVIDE: return 0x4B;
    case SDL_SCANCODE_KP_ENTER: return 0x4C;
    case SDL_SCANCODE_KP_MINUS: return 0x4E;
    case SDL_SCANCODE_KP_EQUALS: return 0x51;
    case SDL_SCANCODE_KP_0: return 0x52;
    case SDL_SCANCODE_KP_1: return 0x53;
    case SDL_SCANCODE_KP_2: return 0x54;
    case SDL_SCANCODE_KP_3: return 0x55;
    case SDL_SCANCODE_KP_4: return 0x56;
    case SDL_SCANCODE_KP_5: return 0x57;
    case SDL_SCANCODE_KP_6: return 0x58;
    case SDL_SCANCODE_KP_7: return 0x59;
    case SDL_SCANCODE_F18: return 0x4F;
    case SDL_SCANCODE_F19: return 0x50;
    case SDL_SCANCODE_KP_8: return 0x5B;
    case SDL_SCANCODE_KP_9: return 0x5C;
    case SDL_SCANCODE_F5: return 0x60;
    case SDL_SCANCODE_F6: return 0x61;
    case SDL_SCANCODE_F7: return 0x62;
    case SDL_SCANCODE_F3: return 0x63;
    case SDL_SCANCODE_F8: return 0x64;
    case SDL_SCANCODE_F9: return 0x65;
    case SDL_SCANCODE_F11: return 0x67;
    case SDL_SCANCODE_F13: return 0x69;
    case SDL_SCANCODE_F16: return 0x6A;
    case SDL_SCANCODE_F14: return 0x6B;
    case SDL_SCANCODE_F10: return 0x6D;
    case SDL_SCANCODE_F12: return 0x6F;
    case SDL_SCANCODE_F15: return 0x71;
    case SDL_SCANCODE_INSERT: return 0x72;
    case SDL_SCANCODE_HOME: return 0x73;
    case SDL_SCANCODE_PAGEUP: return 0x74;
    case SDL_SCANCODE_DELETE: return 0x75;
    case SDL_SCANCODE_F4: return 0x76;
    case SDL_SCANCODE_END: return 0x77;
    case SDL_SCANCODE_F2: return 0x78;
    case SDL_SCANCODE_PAGEDOWN: return 0x79;
    case SDL_SCANCODE_F1: return 0x7A;
    case SDL_SCANCODE_LEFT: return 0x7B;
    case SDL_SCANCODE_RIGHT: return 0x7C;
    case SDL_SCANCODE_DOWN: return 0x7D;
    case SDL_SCANCODE_UP: return 0x7E;
    default: return 0xFFFF;
    }
}

static void tb_disp_queue_input_event(struct tb_display *d, const struct tb_input_event *event) {
    if (!d || !event) return;
    (void)tb_input_queue_push(&d->input_queue, event);
}

static void tb_disp_destroy_status_texture(struct tb_display *d) {
    if (d->status_tex) {
        SDL_DestroyTexture(d->status_tex);
        d->status_tex = NULL;
    }
}

/* "system" and "system-bold" select the macOS UI font (San Francisco on
 * 10.11+); anything else is a PostScript font name. */
static CTFontRef tb_disp_create_font(const char *font_name, CGFloat font_size) {
    if (strcmp(font_name, "system") == 0) {
        return CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, font_size, NULL);
    }
    if (strcmp(font_name, "system-bold") == 0) {
        return CTFontCreateUIFontForLanguage(kCTFontUIFontEmphasizedSystem, font_size, NULL);
    }
    CFStringRef cf_font_name = CFStringCreateWithCString(NULL, font_name, kCFStringEncodingUTF8);
    if (!cf_font_name) return NULL;
    CTFontRef font = CTFontCreateWithName(cf_font_name, font_size, NULL);
    CFRelease(cf_font_name);
    return font;
}

static CTLineRef tb_disp_create_line(const char *text,
                                     const char *font_name,
                                     CGFloat font_size,
                                     CGFloat r,
                                     CGFloat g,
                                     CGFloat b,
                                     CGFloat kern) {
    CFStringRef cf_text = CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
    if (!cf_text) return NULL;

    CTFontRef font = tb_disp_create_font(font_name, font_size);
    if (!font) {
        CFRelease(cf_text);
        return NULL;
    }

    CGFloat components[4] = { r, g, b, 1.0 };
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGColorRef color = CGColorCreate(color_space, components);
    CGColorSpaceRelease(color_space);
    if (!color) {
        CFRelease(font);
        CFRelease(cf_text);
        return NULL;
    }

    CFNumberRef kern_number = CFNumberCreate(NULL, kCFNumberCGFloatType, &kern);
    const void *keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName, kCTKernAttributeName };
    const void *values[] = { font, color, kern_number };
    CFDictionaryRef attrs = CFDictionaryCreate(
        NULL,
        keys,
        values,
        kern_number ? 3 : 2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    CFAttributedStringRef attributed = attrs ? CFAttributedStringCreate(NULL, cf_text, attrs) : NULL;
    CTLineRef line = attributed ? CTLineCreateWithAttributedString(attributed) : NULL;

    if (attributed) CFRelease(attributed);
    if (attrs) CFRelease(attrs);
    if (kern_number) CFRelease(kern_number);
    CGColorRelease(color);
    CFRelease(font);
    CFRelease(cf_text);
    return line;
}

enum tb_disp_align {
    TB_ALIGN_LEFT = 0,
    TB_ALIGN_CENTER = 1,
    TB_ALIGN_RIGHT = 2
};

/* Draws one line of text with its baseline at y (top-left canvas origin).
 * align picks what x means: the left edge, the centre or the right edge.
 * A max_width above zero shrinks the font until the line fits. Returns the
 * drawn width. */
static CGFloat tb_disp_draw_text_ex(CGContextRef ctx,
                                    const char *text,
                                    const char *font_name,
                                    CGFloat font_size,
                                    CGFloat x,
                                    CGFloat y,
                                    CGFloat r,
                                    CGFloat g,
                                    CGFloat b,
                                    int align,
                                    CGFloat max_width,
                                    CGFloat kern) {
    if (!text || !*text) return 0.0;

    CTLineRef line = tb_disp_create_line(text, font_name, font_size, r, g, b, kern);
    if (!line) return 0.0;
    CGFloat width = (CGFloat)CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    if (max_width > 0.0 && width > max_width && width > 0.0) {
        const CGFloat fitted = floor(font_size * max_width / width * 10.0) / 10.0;
        CFRelease(line);
        line = tb_disp_create_line(text, font_name, fitted, r, g, b, kern);
        if (!line) return 0.0;
        width = (CGFloat)CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    }

    CGFloat left = x;
    if (align == TB_ALIGN_CENTER) left = x - width / 2.0;
    else if (align == TB_ALIGN_RIGHT) left = x - width;

    CGContextSaveGState(ctx);
    /* The whole status canvas is flipped to get a top-left origin.
     * CoreText glyph rasterization does not follow that transform the
     * way the filled rects do, so we locally flip again just for text. */
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextTranslateCTM(ctx, 0.0, 2.0 * y);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextSetTextPosition(ctx, left, y);
    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);
    CFRelease(line);
    return width;
}

static CGFloat tb_disp_text_width(const char *text, const char *font_name, CGFloat font_size, CGFloat kern) {
    if (!text || !*text) return 0.0;
    CTLineRef line = tb_disp_create_line(text, font_name, font_size, 0, 0, 0, kern);
    if (!line) return 0.0;
    const CGFloat width = (CGFloat)CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    CFRelease(line);
    return width;
}

static void tb_disp_fill_rect(CGContextRef ctx,
                              CGFloat x,
                              CGFloat y,
                              CGFloat w,
                              CGFloat h,
                              CGFloat r,
                              CGFloat g,
                              CGFloat b,
                              CGFloat a) {
    CGContextSetRGBFillColor(ctx, r, g, b, a);
    CGContextFillRect(ctx, CGRectMake(x, y, w, h));
}

static void tb_disp_fill_rounded_rect(CGContextRef ctx,
                                      CGFloat x,
                                      CGFloat y,
                                      CGFloat w,
                                      CGFloat h,
                                      CGFloat radius,
                                      CGFloat r,
                                      CGFloat g,
                                      CGFloat b,
                                      CGFloat a) {
    CGPathRef path = CGPathCreateWithRoundedRect(CGRectMake(x, y, w, h), radius, radius, NULL);
    if (!path) return;
    CGContextSetRGBFillColor(ctx, r, g, b, a);
    CGContextAddPath(ctx, path);
    CGContextFillPath(ctx);
    CGPathRelease(path);
}

static void tb_disp_stroke_rounded_rect(CGContextRef ctx,
                                        CGFloat x,
                                        CGFloat y,
                                        CGFloat w,
                                        CGFloat h,
                                        CGFloat radius,
                                        CGFloat line_width,
                                        CGFloat r,
                                        CGFloat g,
                                        CGFloat b,
                                        CGFloat a) {
    CGPathRef path = CGPathCreateWithRoundedRect(CGRectMake(x, y, w, h), radius, radius, NULL);
    if (!path) return;
    CGContextSetRGBStrokeColor(ctx, r, g, b, a);
    CGContextSetLineWidth(ctx, line_width);
    CGContextAddPath(ctx, path);
    CGContextStrokePath(ctx);
    CGPathRelease(path);
}

/* The Receiver's own app icon from its bundle; NULL when running unbundled. */
static CGImageRef tb_disp_load_app_icon(void) {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle) return NULL;
    CFURLRef url = CFBundleCopyResourceURL(bundle, CFSTR("TargetBridgeReceiver"), CFSTR("icns"), NULL);
    if (!url) return NULL;
    CGImageSourceRef source = CGImageSourceCreateWithURL(url, NULL);
    CFRelease(url);
    if (!source) return NULL;

    CGImageRef best = NULL;
    size_t best_width = 0;
    const size_t count = CGImageSourceGetCount(source);
    for (size_t i = 0; i < count; i++) {
        CGImageRef image = CGImageSourceCreateImageAtIndex(source, i, NULL);
        if (!image) continue;
        const size_t width = CGImageGetWidth(image);
        if (width > best_width) {
            if (best) CGImageRelease(best);
            best = image;
            best_width = width;
        } else {
            CGImageRelease(image);
        }
    }
    CFRelease(source);
    return best;
}

static void tb_disp_draw_brand_icon(CGContextRef ctx, CGFloat x, CGFloat y, CGFloat size);

static void tb_disp_draw_app_icon(struct tb_display *d, CGContextRef ctx, CGFloat x, CGFloat y, CGFloat size) {
    if (!d->app_icon_loaded) {
        d->app_icon = tb_disp_load_app_icon();
        d->app_icon_loaded = 1;
    }
    if (!d->app_icon) {
        tb_disp_draw_brand_icon(ctx, x, y, size);
        return;
    }
    CGContextSaveGState(ctx);
    CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
    /* Images draw bottom-up; undo the canvas flip locally. */
    CGContextTranslateCTM(ctx, x, y + size);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextDrawImage(ctx, CGRectMake(0, 0, size, size), d->app_icon);
    CGContextRestoreGState(ctx);
}

static void tb_disp_draw_brand_icon(CGContextRef ctx, CGFloat x, CGFloat y, CGFloat size) {
    const CGFloat inset = size * 0.22;
    const CGFloat monitor_w = size * 0.56;
    const CGFloat monitor_h = size * 0.34;
    const CGFloat monitor_x = x + (size - monitor_w) / 2.0;
    const CGFloat monitor_y = y + size * 0.25;

    tb_disp_fill_rounded_rect(ctx, x, y, size, size, size * 0.22, 0.13, 0.34, 0.24, 1.0);
    CGContextSetRGBStrokeColor(ctx, 0.94, 0.98, 0.96, 1.0);
    CGContextSetLineWidth(ctx, size * 0.045);
    CGContextStrokeRect(ctx, CGRectMake(monitor_x, monitor_y, monitor_w, monitor_h));
    CGContextMoveToPoint(ctx, monitor_x + monitor_w * 0.5, monitor_y + monitor_h);
    CGContextAddLineToPoint(ctx, monitor_x + monitor_w * 0.5, monitor_y + monitor_h + size * 0.13);
    CGContextMoveToPoint(ctx, monitor_x + inset, monitor_y + monitor_h + size * 0.13);
    CGContextAddLineToPoint(ctx, monitor_x + monitor_w - inset, monitor_y + monitor_h + size * 0.13);
    CGContextStrokePath(ctx);
}

static void tb_disp_draw_connecting_spinner(struct tb_display *d, int drawable_w, int drawable_h) {
    if (!d || !d->ren) return;

    static const int vectors[12][2] = {
        { 0, -100 }, { 50, -87 }, { 87, -50 }, { 100, 0 },
        { 87, 50 }, { 50, 87 }, { 0, 100 }, { -50, 87 },
        { -87, 50 }, { -100, 0 }, { -87, -50 }, { -50, -87 }
    };
    const int min_side = drawable_w < drawable_h ? drawable_w : drawable_h;
    const int radius = min_side / 13;
    const int segment = min_side / 42;
    const int center_x = drawable_w / 2;
    const int center_y = drawable_h / 2 + min_side / 5;
    const int phase = (int)((SDL_GetTicks() / 90u) % 12u);

    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(d->ren, &old_blend);
    SDL_SetRenderDrawBlendMode(d->ren, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 12; i++) {
        const int age = (i - phase + 12) % 12;
        const Uint8 alpha = (Uint8)(54 + (11 - age) * 18);
        const int x1 = center_x + vectors[i][0] * radius / 100;
        const int y1 = center_y + vectors[i][1] * radius / 100;
        const int x2 = center_x + vectors[i][0] * (radius + segment) / 100;
        const int y2 = center_y + vectors[i][1] * (radius + segment) / 100;
        SDL_SetRenderDrawColor(d->ren, 84, 225, 137, alpha);
        SDL_RenderDrawLine(d->ren, x1, y1, x2, y2);
    }
    SDL_SetRenderDrawBlendMode(d->ren, old_blend);
}

/* Permission chips: the text holds one "Name: state" entry per permission,
 * separated by three spaces. Granted ones are green, missing ones amber. */
static void tb_disp_draw_permission_chips(CGContextRef ctx,
                                          const char *permissions,
                                          const char *font_name,
                                          CGFloat font_size,
                                          CGFloat x,
                                          CGFloat baseline,
                                          CGFloat max_width,
                                          CGFloat scale) {
    if (!permissions || !*permissions) return;

    const CGFloat pad = 10.0 * scale;
    const CGFloat gap = 6.0 * scale;
    const CGFloat chip_h = font_size * 1.6;
    const CGFloat line_step = chip_h + gap;
    CGFloat cursor_x = x;
    CGFloat cursor_baseline = baseline;

    const char *p = permissions;
    while (*p) {
        const char *sep = strstr(p, "   ");
        const size_t len = sep ? (size_t)(sep - p) : strlen(p);
        char part[96];
        const size_t n = len < sizeof(part) - 1 ? len : sizeof(part) - 1;
        memcpy(part, p, n);
        part[n] = '\0';

        if (part[0] != '\0') {
            const int granted = strstr(part, "OK") != NULL || strstr(part, "\xE6\xAD\xA3\xE5\xB8\xB8") != NULL;
            const CGFloat text_w = tb_disp_text_width(part, font_name, font_size, 0.0);
            const CGFloat chip_w = text_w + 2.0 * pad;
            if (cursor_x > x && cursor_x + chip_w > x + max_width) {
                cursor_x = x;
                cursor_baseline += line_step;
            }
            const CGFloat chip_top = cursor_baseline - font_size * 1.12;
            if (granted) {
                tb_disp_fill_rounded_rect(ctx, cursor_x, chip_top, chip_w, chip_h, chip_h / 2.0,
                                          0.12, 0.24, 0.15, 1.0);
                tb_disp_draw_text_ex(ctx, part, font_name, font_size, cursor_x + pad, cursor_baseline,
                                     0.36, 0.89, 0.48, TB_ALIGN_LEFT, 0.0, 0.0);
            } else {
                tb_disp_fill_rounded_rect(ctx, cursor_x, chip_top, chip_w, chip_h, chip_h / 2.0,
                                          0.23, 0.165, 0.07, 1.0);
                tb_disp_draw_text_ex(ctx, part, font_name, font_size, cursor_x + pad, cursor_baseline,
                                     0.96, 0.72, 0.25, TB_ALIGN_LEFT, 0.0, 0.0);
            }
            cursor_x += chip_w + gap;
        }

        if (!sep) break;
        p = sep;
        while (*p == ' ') p++;
    }
}

static void tb_disp_rebuild_status_texture(struct tb_display *d,
                                           const char *ip,
                                           const char *address_label,
                                           const char *status,
                                           const char *sender,
                                           const char *panel,
                                           const char *mode,
                                           const char *language,
                                           const char *permissions,
                                           int drawable_w,
                                           int drawable_h,
                                           int connecting) {
    if (!d || drawable_w <= 0 || drawable_h <= 0) return;

    tb_disp_destroy_status_texture(d);

    const size_t bytes_per_row = (size_t)drawable_w * 4u;
    uint32_t *pixels = (uint32_t *)calloc((size_t)drawable_w * (size_t)drawable_h, sizeof(uint32_t));
    if (!pixels) return;

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        pixels,
        (size_t)drawable_w,
        (size_t)drawable_h,
        8,
        bytes_per_row,
        color_space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
    );
    CGColorSpaceRelease(color_space);
    if (!ctx) {
        free(pixels);
        return;
    }

    CGContextTranslateCTM(ctx, 0, (CGFloat)drawable_h);
    CGContextScaleCTM(ctx, 1.0, -1.0);

    const char *current_language = tb_i18n_current_language();
    const int zh = current_language && strncmp(current_language, "zh", 2) == 0;
    const char *title_font = zh ? "PingFangSC-Semibold" : "system-bold";
    const char *body_font = zh ? "PingFangSC-Regular" : "system";
    const char *mono_font = "Menlo";
    const char *mono_bold_font = "Menlo-Bold";

    /* Every size below is in points of a 1600 x 900 design, scaled to the
     * real drawable, so the screen reads the same on 1440p and 5K panels. */
    const CGFloat w = (CGFloat)drawable_w;
    const CGFloat h = (CGFloat)drawable_h;
    const CGFloat s = fmin(w / 1600.0, h / 900.0);
    const CGFloat cx = w / 2.0;

    /* Palette: near-black ground, primary/secondary text, green accent. */
    tb_disp_fill_rect(ctx, 0, 0, w, h, 0.059, 0.063, 0.071, 1.0);

    if (connecting) {
        /* The SDL spinner sits at h/2 + min_side/5, below this stack. */
        CGFloat y = h / 2.0 - 230.0 * s;
        const CGFloat icon_size = 112.0 * s;
        tb_disp_draw_app_icon(d, ctx, cx - icon_size / 2.0, y, icon_size);
        y += icon_size + 52.0 * s;
        tb_disp_draw_text_ex(ctx, "TargetBridge", title_font, 40.0 * s, cx, y,
                             0.96, 0.96, 0.97, TB_ALIGN_CENTER, 0.0, 0.0);
        y += 30.0 * s;
        tb_disp_draw_text_ex(ctx, "RECEIVER", title_font, 14.0 * s, cx, y,
                             0.557, 0.557, 0.576, TB_ALIGN_CENTER, 0.0, 2.0 * s);
        y += 56.0 * s;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.splash.connecting"), title_font, 24.0 * s, cx, y,
                             0.96, 0.96, 0.97, TB_ALIGN_CENTER, w - 240.0 * s, 0.0);
        y += 34.0 * s;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.splash.waiting_first_frame"), body_font, 17.0 * s, cx, y,
                             0.63, 0.63, 0.65, TB_ALIGN_CENTER, w - 240.0 * s, 0.0);
        tb_disp_draw_text_ex(ctx, TB_RECEIVER_VERSION, mono_font, 13.0 * s, cx, h - 40.0 * s,
                             0.557, 0.557, 0.576, TB_ALIGN_CENTER, 0.0, 0.0);
    } else {
        const CGFloat footer_h = 150.0 * s;
        const CGFloat footer_top = h - footer_h;
        const CGFloat max_text = w - 240.0 * s;
        const CGFloat stack_h = 470.0 * s;
        CGFloat y = fmax(30.0 * s, (footer_top - stack_h) / 2.0);

        /* Centre: icon, title, headline, address, status, help. */
        const CGFloat icon_size = 96.0 * s;
        tb_disp_draw_app_icon(d, ctx, cx - icon_size / 2.0, y, icon_size);
        y += icon_size + 36.0 * s;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.title"), title_font, 14.0 * s, cx, y,
                             0.557, 0.557, 0.576, TB_ALIGN_CENTER, max_text, 2.0 * s);
        y += 50.0 * s;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.subtitle"), title_font, 40.0 * s, cx, y,
                             0.96, 0.96, 0.97, TB_ALIGN_CENTER, max_text, 0.0);
        y += 70.0 * s;
        tb_disp_draw_text_ex(ctx, ip, mono_bold_font, 46.0 * s, cx, y,
                             0.357, 0.89, 0.478, TB_ALIGN_CENTER, max_text, 0.0);
        y += 30.0 * s;
        tb_disp_draw_text_ex(ctx, address_label, title_font, 13.0 * s, cx, y,
                             0.557, 0.557, 0.576, TB_ALIGN_CENTER, max_text, 1.5 * s);
        y += 26.0 * s;

        const int waiting = strcmp(status, tb_i18n_get("receiver.status.waiting_for_sender")) == 0;
        const CGFloat pill_font = 17.0 * s;
        const CGFloat dot = 10.0 * s;
        const CGFloat pill_pad = 18.0 * s;
        const CGFloat pill_h = 38.0 * s;
        const CGFloat status_w = fmin(tb_disp_text_width(status, body_font, pill_font, 0.0), max_text);
        const CGFloat pill_w = pill_pad + dot + 10.0 * s + status_w + pill_pad;
        const CGFloat pill_x = cx - pill_w / 2.0;
        tb_disp_fill_rounded_rect(ctx, pill_x, y, pill_w, pill_h, pill_h / 2.0, 0.122, 0.125, 0.137, 1.0);
        tb_disp_stroke_rounded_rect(ctx, pill_x, y, pill_w, pill_h, pill_h / 2.0, fmax(1.0, s),
                                    0.2, 0.2, 0.22, 1.0);
        if (waiting) {
            CGContextSetRGBFillColor(ctx, 0.96, 0.72, 0.25, 1.0);
        } else {
            CGContextSetRGBFillColor(ctx, 0.357, 0.89, 0.478, 1.0);
        }
        CGContextFillEllipseInRect(ctx, CGRectMake(pill_x + pill_pad, y + (pill_h - dot) / 2.0, dot, dot));
        tb_disp_draw_text_ex(ctx, status, body_font, pill_font, pill_x + pill_pad + dot + 10.0 * s, y + 25.0 * s,
                             0.96, 0.96, 0.97, TB_ALIGN_LEFT, max_text, 0.0);
        y += pill_h + 40.0 * s;

        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.help_1"), body_font, 17.0 * s, cx, y,
                             0.63, 0.63, 0.65, TB_ALIGN_CENTER, max_text, 0.0);
        y += 28.0 * s;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.help_2"), body_font, 17.0 * s, cx, y,
                             0.63, 0.63, 0.65, TB_ALIGN_CENTER, max_text, 0.0);
        y += 28.0 * s;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.help_3"), body_font, 17.0 * s, cx, y,
                             0.63, 0.63, 0.65, TB_ALIGN_CENTER, max_text, 0.0);

        /* Footer: five columns of details along the bottom edge. */
        tb_disp_fill_rect(ctx, 0, footer_top, w, fmax(1.0, s), 0.15, 0.15, 0.165, 1.0);
        const CGFloat margin = 40.0 * s;
        const CGFloat col_w = (w - 2.0 * margin) / 5.0;
        const CGFloat col_max = col_w - 16.0 * s;
        const CGFloat label_y = footer_top + 36.0 * s;
        const CGFloat value_y = footer_top + 66.0 * s;
        const CGFloat label_size = 11.0 * s;
        const CGFloat label_kern = 1.2 * s;

        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.permissions"), title_font, label_size,
                             margin, label_y, 0.557, 0.557, 0.576, TB_ALIGN_LEFT, col_max, label_kern);
        tb_disp_draw_permission_chips(ctx, permissions, body_font, 13.0 * s, margin, value_y, col_max, s);

        const CGFloat sender_x = margin + col_w;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.sender"), title_font, label_size,
                             sender_x, label_y, 0.557, 0.557, 0.576, TB_ALIGN_LEFT, col_max, label_kern);
        tb_disp_draw_text_ex(ctx, sender, body_font, 15.0 * s, sender_x, value_y,
                             0.96, 0.96, 0.97, TB_ALIGN_LEFT, col_max, 0.0);

        const CGFloat mode_x = margin + 2.0 * col_w;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.stream_profile"), title_font, label_size,
                             mode_x, label_y, 0.557, 0.557, 0.576, TB_ALIGN_LEFT, col_max, label_kern);
        tb_disp_draw_text_ex(ctx, mode, zh ? body_font : mono_font, 14.0 * s, mode_x, value_y,
                             0.96, 0.96, 0.97, TB_ALIGN_LEFT, col_max, 0.0);

        const CGFloat panel_x = margin + 3.0 * col_w;
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.display"), title_font, label_size,
                             panel_x, label_y, 0.557, 0.557, 0.576, TB_ALIGN_LEFT, col_max, label_kern);
        tb_disp_draw_text_ex(ctx, panel, zh ? body_font : mono_font, 14.0 * s, panel_x, value_y,
                             0.96, 0.96, 0.97, TB_ALIGN_LEFT, col_max, 0.0);

        const CGFloat right_x = w - margin;
        char version_line[96];
        snprintf(version_line, sizeof(version_line), "%s · %s", TB_RECEIVER_VERSION, TB_RECEIVER_BUILD);
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.language"), title_font, label_size,
                             right_x, label_y, 0.557, 0.557, 0.576, TB_ALIGN_RIGHT, col_max, label_kern);
        tb_disp_draw_text_ex(ctx, language, body_font, 15.0 * s, right_x, value_y,
                             0.96, 0.96, 0.97, TB_ALIGN_RIGHT, col_max, 0.0);
        tb_disp_draw_text_ex(ctx, tb_i18n_get("receiver.ui.help_4"), body_font, 13.0 * s, right_x, value_y + 24.0 * s,
                             0.557, 0.557, 0.576, TB_ALIGN_RIGHT, col_max, 0.0);
        tb_disp_draw_text_ex(ctx, version_line, mono_font, 12.0 * s, right_x, value_y + 46.0 * s,
                             0.557, 0.557, 0.576, TB_ALIGN_RIGHT, col_max, 0.0);
    }

    CGContextRelease(ctx);

    d->status_tex = SDL_CreateTexture(
        d->ren,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,
        drawable_w,
        drawable_h
    );
    if (d->status_tex) {
        SDL_UpdateTexture(d->status_tex, NULL, pixels, (int)bytes_per_row);
        SDL_SetTextureBlendMode(d->status_tex, SDL_BLENDMODE_NONE);
    }
    free(pixels);
}

static void tb_disp_refresh_window_mode(struct tb_display *d) {
    if (!d || !d->win) return;

    const int monitor_shield_active =
        (d->is_connected || d->is_connecting) && d->preferred_fullscreen;

    if (monitor_shield_active) {
        SDL_SetWindowFullscreen(d->win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(d->win, 0);
        SDL_SetWindowSize(d->win, 980, 620);
        SDL_SetWindowPosition(d->win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    tb_receiver_set_monitor_shield(monitor_shield_active);

    const int should_hide_cursor =
        ((d->is_connected || d->is_connecting) && d->preferred_fullscreen) ||
        d->input_capture_active ||
        d->input_intercept_active;

    SDL_ShowCursor(should_hide_cursor ? SDL_DISABLE : SDL_ENABLE);
    if (should_hide_cursor && !d->system_cursor_hidden) {
        CGDisplayHideCursor(CGMainDisplayID());
        d->system_cursor_hidden = 1;
    } else if (!should_hide_cursor && d->system_cursor_hidden) {
        CGDisplayShowCursor(CGMainDisplayID());
        d->system_cursor_hidden = 0;
    }
}

static SDL_Renderer *tb_disp_try_renderer(SDL_Window *win, const char *driver) {
    if (driver && driver[0] != '\0') {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, driver);
    } else {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "");
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) {
        fprintf(stderr, "[disp] CreateRenderer(%s): %s\n",
                driver && driver[0] != '\0' ? driver : "default",
                SDL_GetError());
    }
    return ren;
}

static SDL_Renderer *tb_disp_create_accelerated_renderer(SDL_Window *win) {
    const char *forced_driver = getenv("TB_RECEIVER_RENDER_DRIVER");
    if (forced_driver && forced_driver[0] != '\0') {
        fprintf(stderr, "[disp] renderer override = %s\n", forced_driver);
        return tb_disp_try_renderer(win, forced_driver);
    }

#if defined(__APPLE__)
    const char *macos_drivers[] = { "opengl", "metal", NULL };
    for (int i = 0; macos_drivers[i] != NULL; i++) {
        SDL_Renderer *ren = tb_disp_try_renderer(win, macos_drivers[i]);
        if (ren) return ren;
    }
#endif

    return tb_disp_try_renderer(win, NULL);
}

struct tb_display *tb_disp_create(int fullscreen) {
    /* Best-quality scaling (linear filter; Metal backend uses bilinear
     * regardless but this sets the hint correctly). "best" enables
     * anisotropic where supported. Must be set BEFORE renderer creation. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[disp] SDL_Init: %s\n", SDL_GetError());
        return NULL;
    }

    struct tb_display *d = (struct tb_display *)calloc(1, sizeof(*d));
    if (!d) return NULL;

    Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;

    d->win = SDL_CreateWindow("TargetBridge Receiver",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              980, 620, flags);
    if (!d->win) {
        fprintf(stderr, "[disp] CreateWindow: %s\n", SDL_GetError());
        free(d); return NULL;
    }

    /* No VSYNC: lets us present as fast as decode produces frames.
     * On Intel iMac with Radeon Pro 570 + 5K display, VSYNC at 60Hz combined
     * with GPU→CPU NV12 transfer was stalling the pipeline to ~4 fps. */
    d->ren = tb_disp_create_accelerated_renderer(d->win);
    if (!d->ren) {
        fprintf(stderr, "[disp] CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(d->win); free(d); return NULL;
    }
    SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_BT709);
    SDL_RenderSetLogicalSize(d->ren, 0, 0);

    /* report which backend SDL picked */
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(d->ren, &info) == 0) {
        fprintf(stderr, "[disp] renderer = %s\n", info.name);
    }

    int win_w = 0, win_h = 0, out_w = 0, out_h = 0;
    SDL_GetWindowSize(d->win, &win_w, &win_h);
    if (SDL_GetRendererOutputSize(d->ren, &out_w, &out_h) == 0) {
        fprintf(stderr, "[disp] window %dx%d -> drawable %dx%d\n",
                win_w, win_h, out_w, out_h);
    }

    d->preferred_fullscreen = fullscreen;
    d->is_connected = 0;
    d->last_ip[0] = '\0';
    d->last_status[0] = '\0';
    d->last_sender[0] = '\0';
    d->last_panel[0] = '\0';
    d->last_mode[0] = '\0';
    d->last_language[0] = '\0';
    d->last_drawable_w = 0;
    d->last_drawable_h = 0;
    d->cursor_x = 0;
    d->cursor_y = 0;
    d->cursor_source_w = 1;
    d->cursor_source_h = 1;
    d->cursor_visible = 0;
    d->cursor_type = 0;
    d->system_cursor_hidden = 0;
    d->last_video_frame_time = 0;

    tb_disp_refresh_window_mode(d);
    SDL_ShowWindow(d->win);
    return d;
}

void tb_disp_destroy(struct tb_display *d) {
    if (!d) return;
    if (d->system_cursor_hidden) {
        CGDisplayShowCursor(CGMainDisplayID());
        d->system_cursor_hidden = 0;
    }
    tb_disp_destroy_status_texture(d);
    if (d->app_icon) CGImageRelease(d->app_icon);
    if (d->tex) SDL_DestroyTexture(d->tex);
    if (d->ren) SDL_DestroyRenderer(d->ren);
    if (d->win) SDL_DestroyWindow(d->win);
    SDL_EnableScreenSaver();
    SDL_Quit();
    free(d);
}

int tb_disp_ensure_texture(struct tb_display *d, int w, int h) {
    if (d->tex && d->tex_w == w && d->tex_h == h) return 0;
    if (d->tex) { SDL_DestroyTexture(d->tex); d->tex = NULL; }

    d->tex = SDL_CreateTexture(d->ren, SDL_PIXELFORMAT_NV12,
                               SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!d->tex) {
        fprintf(stderr, "[disp] CreateTexture(NV12): %s\n", SDL_GetError());
        return -1;
    }
    d->tex_w = w;
    d->tex_h = h;
    fprintf(stderr, "[disp] texture %dx%d NV12\n", w, h);
    return 0;
}

static void tb_disp_draw_poly_outline(SDL_Renderer *ren, const SDL_Point *pts, int count) {
    if (!ren || !pts || count < 2) return;
    for (int i = 0; i < count; ++i) {
        const SDL_Point a = pts[i];
        const SDL_Point b = pts[(i + 1) % count];
        SDL_RenderDrawLine(ren, a.x, a.y, b.x, b.y);
    }
}

static void tb_disp_fill_poly(SDL_Renderer *ren, const SDL_Point *pts, int count) {
    if (!ren || !pts || count < 3) return;

    int min_y = pts[0].y;
    int max_y = pts[0].y;
    for (int i = 1; i < count; ++i) {
        if (pts[i].y < min_y) min_y = pts[i].y;
        if (pts[i].y > max_y) max_y = pts[i].y;
    }

    for (int y = min_y; y <= max_y; ++y) {
        int nodes[16];
        int node_count = 0;

        for (int i = 0, j = count - 1; i < count; j = i++) {
            const int yi = pts[i].y;
            const int yj = pts[j].y;
            if ((yi < y && yj >= y) || (yj < y && yi >= y)) {
                const int xi = pts[i].x;
                const int xj = pts[j].x;
                if (node_count < (int)(sizeof(nodes) / sizeof(nodes[0]))) {
                    nodes[node_count++] = xi + ((y - yi) * (xj - xi)) / (yj - yi);
                }
            }
        }

        for (int i = 1; i < node_count; ++i) {
            int v = nodes[i];
            int k = i - 1;
            while (k >= 0 && nodes[k] > v) {
                nodes[k + 1] = nodes[k];
                --k;
            }
            nodes[k + 1] = v;
        }

        for (int i = 0; i + 1 < node_count; i += 2) {
            SDL_RenderDrawLine(ren, nodes[i], y, nodes[i + 1], y);
        }
    }
}

static int tb_disp_cursor_scale(int size, int value) {
    return (size * value + 16) / 32;
}

static SDL_Point tb_disp_cursor_point(int x, int y, int size, int px, int py) {
    SDL_Point p = {
        x + tb_disp_cursor_scale(size, px),
        y + tb_disp_cursor_scale(size, py)
    };
    return p;
}

static void draw_scaled_rect(SDL_Renderer *ren, int cx, int cy, int size, int rx1, int ry1, int rx2, int ry2) {
    SDL_Rect r;
    int x1 = cx + tb_disp_cursor_scale(size, rx1);
    int y1 = cy + tb_disp_cursor_scale(size, ry1);
    int x2 = cx + tb_disp_cursor_scale(size, rx2);
    int y2 = cy + tb_disp_cursor_scale(size, ry2);
    r.x = x1 < x2 ? x1 : x2;
    r.y = y1 < y2 ? y1 : y2;
    r.w = x2 > x1 ? (x2 - x1) : (x1 - x2);
    r.h = y2 > y1 ? (y2 - y1) : (y1 - y2);
    if (r.w == 0) r.w = 1;
    if (r.h == 0) r.h = 1;
    SDL_RenderFillRect(ren, &r);
}

static void tb_disp_draw_cursor(struct tb_display *d) {
    if (!d || !d->cursor_visible || d->cursor_source_w <= 0 || d->cursor_source_h <= 0) return;

    int out_w = 0, out_h = 0;
    if (SDL_GetRendererOutputSize(d->ren, &out_w, &out_h) < 0 || out_w <= 0 || out_h <= 0) {
        return;
    }

    const double sx = (double)out_w / (double)d->cursor_source_w;
    const double sy = (double)out_h / (double)d->cursor_source_h;
    const int x = (int)((double)d->cursor_x * sx);
    const int y = (int)((double)d->cursor_y * sy);
    const int size = d->cursor_large
        ? (out_w >= 5000 ? 58 : 44)
        : (out_w >= 5000 ? 32 : 24);
    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    (void)SDL_GetRenderDrawBlendMode(d->ren, &old_blend);

    SDL_SetRenderDrawBlendMode(d->ren, SDL_BLENDMODE_BLEND);

    if (d->cursor_type == 1) {
        /* I-Beam (Text Cursor) */
        /* Draw Shadow */
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        int sx_off = 1, sy_off = 2;
        draw_scaled_rect(d->ren, x + sx_off, y + sy_off, size, -8, -14, 8, -10);
        draw_scaled_rect(d->ren, x + sx_off, y + sy_off, size, -8, 10, 8, 14);
        draw_scaled_rect(d->ren, x + sx_off, y + sy_off, size, -2, -12, 2, 12);
        
        /* Draw Outline (White) */
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        draw_scaled_rect(d->ren, x, y, size, -8, -14, 8, -10);
        draw_scaled_rect(d->ren, x, y, size, -8, 10, 8, 14);
        draw_scaled_rect(d->ren, x, y, size, -2, -12, 2, 12);
        
        /* Draw Body (Black) */
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        draw_scaled_rect(d->ren, x, y, size, -7, -13, 7, -11);
        draw_scaled_rect(d->ren, x, y, size, -7, 11, 7, 13);
        draw_scaled_rect(d->ren, x, y, size, -1, -11, 1, 11);
    }
    else if (d->cursor_type == 2) {
        /* Pointing Hand */
        const int cx = x - tb_disp_cursor_scale(size, 10);
        const int cy = y;
        SDL_Point shadow[] = {
            tb_disp_cursor_point(cx + 2, cy + 3, size, 10, 0),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 13, 2),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 13, 11),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 17, 11),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 17, 15),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 16, 17),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 16, 20),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 14, 22),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 14, 24),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 11, 27),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 5, 27),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 1, 20),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 0, 16),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 3, 13),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 7, 11),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 7, 2)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        tb_disp_fill_poly(d->ren, shadow, (int)(sizeof(shadow) / sizeof(shadow[0])));

        SDL_Point outline[] = {
            tb_disp_cursor_point(cx, cy, size, 10, 0),
            tb_disp_cursor_point(cx, cy, size, 13, 2),
            tb_disp_cursor_point(cx, cy, size, 13, 11),
            tb_disp_cursor_point(cx, cy, size, 17, 11),
            tb_disp_cursor_point(cx, cy, size, 17, 15),
            tb_disp_cursor_point(cx, cy, size, 16, 17),
            tb_disp_cursor_point(cx, cy, size, 16, 20),
            tb_disp_cursor_point(cx, cy, size, 14, 22),
            tb_disp_cursor_point(cx, cy, size, 14, 24),
            tb_disp_cursor_point(cx, cy, size, 11, 27),
            tb_disp_cursor_point(cx, cy, size, 5, 27),
            tb_disp_cursor_point(cx, cy, size, 1, 20),
            tb_disp_cursor_point(cx, cy, size, 0, 16),
            tb_disp_cursor_point(cx, cy, size, 3, 13),
            tb_disp_cursor_point(cx, cy, size, 7, 11),
            tb_disp_cursor_point(cx, cy, size, 7, 2)
        };
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        tb_disp_fill_poly(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));
        tb_disp_draw_poly_outline(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));

        SDL_Point body[] = {
            tb_disp_cursor_point(cx, cy, size, 10, 2),
            tb_disp_cursor_point(cx, cy, size, 12, 3),
            tb_disp_cursor_point(cx, cy, size, 12, 11),
            tb_disp_cursor_point(cx, cy, size, 15, 12),
            tb_disp_cursor_point(cx, cy, size, 15, 14),
            tb_disp_cursor_point(cx, cy, size, 14, 17),
            tb_disp_cursor_point(cx, cy, size, 14, 19),
            tb_disp_cursor_point(cx, cy, size, 12, 21),
            tb_disp_cursor_point(cx, cy, size, 12, 23),
            tb_disp_cursor_point(cx, cy, size, 10, 25),
            tb_disp_cursor_point(cx, cy, size, 6, 25),
            tb_disp_cursor_point(cx, cy, size, 3, 19),
            tb_disp_cursor_point(cx, cy, size, 2, 16),
            tb_disp_cursor_point(cx, cy, size, 4, 14),
            tb_disp_cursor_point(cx, cy, size, 8, 12),
            tb_disp_cursor_point(cx, cy, size, 8, 3)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        tb_disp_fill_poly(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
        tb_disp_draw_poly_outline(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
    }
    else if (d->cursor_type == 3) {
        /* Resize Horizontal */
        const int cx = x - tb_disp_cursor_scale(size, 16);
        const int cy = y - tb_disp_cursor_scale(size, 16);
        SDL_Point shadow[] = {
            tb_disp_cursor_point(cx + 2, cy + 3, size, 2, 16),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 8, 10),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 8, 13),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 24, 13),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 24, 10),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 30, 16),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 24, 22),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 24, 19),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 8, 19),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 8, 22)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        tb_disp_fill_poly(d->ren, shadow, (int)(sizeof(shadow) / sizeof(shadow[0])));

        SDL_Point outline[] = {
            tb_disp_cursor_point(cx, cy, size, 2, 16),
            tb_disp_cursor_point(cx, cy, size, 8, 10),
            tb_disp_cursor_point(cx, cy, size, 8, 13),
            tb_disp_cursor_point(cx, cy, size, 24, 13),
            tb_disp_cursor_point(cx, cy, size, 24, 10),
            tb_disp_cursor_point(cx, cy, size, 30, 16),
            tb_disp_cursor_point(cx, cy, size, 24, 22),
            tb_disp_cursor_point(cx, cy, size, 24, 19),
            tb_disp_cursor_point(cx, cy, size, 8, 19),
            tb_disp_cursor_point(cx, cy, size, 8, 22)
        };
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        tb_disp_fill_poly(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));
        tb_disp_draw_poly_outline(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));

        SDL_Point body[] = {
            tb_disp_cursor_point(cx, cy, size, 5, 16),
            tb_disp_cursor_point(cx, cy, size, 8, 12),
            tb_disp_cursor_point(cx, cy, size, 8, 15),
            tb_disp_cursor_point(cx, cy, size, 24, 15),
            tb_disp_cursor_point(cx, cy, size, 24, 12),
            tb_disp_cursor_point(cx, cy, size, 27, 16),
            tb_disp_cursor_point(cx, cy, size, 24, 20),
            tb_disp_cursor_point(cx, cy, size, 24, 17),
            tb_disp_cursor_point(cx, cy, size, 8, 17),
            tb_disp_cursor_point(cx, cy, size, 8, 20)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        tb_disp_fill_poly(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
        tb_disp_draw_poly_outline(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
    }
    else if (d->cursor_type == 4) {
        /* Resize Vertical */
        const int cx = x - tb_disp_cursor_scale(size, 16);
        const int cy = y - tb_disp_cursor_scale(size, 16);
        SDL_Point shadow[] = {
            tb_disp_cursor_point(cx + 2, cy + 3, size, 16, 2),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 22, 8),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 19, 8),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 19, 24),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 22, 24),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 16, 30),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 10, 24),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 13, 24),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 13, 8),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 10, 8)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        tb_disp_fill_poly(d->ren, shadow, (int)(sizeof(shadow) / sizeof(shadow[0])));

        SDL_Point outline[] = {
            tb_disp_cursor_point(cx, cy, size, 16, 2),
            tb_disp_cursor_point(cx, cy, size, 22, 8),
            tb_disp_cursor_point(cx, cy, size, 19, 8),
            tb_disp_cursor_point(cx, cy, size, 19, 24),
            tb_disp_cursor_point(cx, cy, size, 22, 24),
            tb_disp_cursor_point(cx, cy, size, 16, 30),
            tb_disp_cursor_point(cx, cy, size, 10, 24),
            tb_disp_cursor_point(cx, cy, size, 13, 24),
            tb_disp_cursor_point(cx, cy, size, 13, 8),
            tb_disp_cursor_point(cx, cy, size, 10, 8)
        };
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        tb_disp_fill_poly(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));
        tb_disp_draw_poly_outline(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));

        SDL_Point body[] = {
            tb_disp_cursor_point(cx, cy, size, 16, 5),
            tb_disp_cursor_point(cx, cy, size, 20, 8),
            tb_disp_cursor_point(cx, cy, size, 17, 8),
            tb_disp_cursor_point(cx, cy, size, 17, 24),
            tb_disp_cursor_point(cx, cy, size, 20, 24),
            tb_disp_cursor_point(cx, cy, size, 16, 27),
            tb_disp_cursor_point(cx, cy, size, 12, 24),
            tb_disp_cursor_point(cx, cy, size, 15, 24),
            tb_disp_cursor_point(cx, cy, size, 15, 8),
            tb_disp_cursor_point(cx, cy, size, 12, 8)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        tb_disp_fill_poly(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
        tb_disp_draw_poly_outline(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
    }
    else if (d->cursor_type == 6) {
        /* Crosshair */
        /* Draw Shadow */
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        int sx_off = 1, sy_off = 2;
        draw_scaled_rect(d->ren, x + sx_off, y + sy_off, size, -15, -2, -2, 2);
        draw_scaled_rect(d->ren, x + sx_off, y + sy_off, size, 2, -2, 15, 2);
        draw_scaled_rect(d->ren, x + sx_off, y + sy_off, size, -2, -15, 2, -2);
        draw_scaled_rect(d->ren, x + sx_off, y + sy_off, size, -2, 2, 2, 15);
        
        /* Draw Outline (White) */
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        draw_scaled_rect(d->ren, x, y, size, -15, -2, -2, 2);
        draw_scaled_rect(d->ren, x, y, size, 2, -2, 15, 2);
        draw_scaled_rect(d->ren, x, y, size, -2, -15, 2, -2);
        draw_scaled_rect(d->ren, x, y, size, -2, 2, 2, 15);
        
        /* Draw Body (Black) */
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        draw_scaled_rect(d->ren, x, y, size, -14, -1, -3, 1);
        draw_scaled_rect(d->ren, x, y, size, 3, -1, 14, 1);
        draw_scaled_rect(d->ren, x, y, size, -1, -14, 1, -3);
        draw_scaled_rect(d->ren, x, y, size, -1, 3, 1, 14);
    }
    else if (d->cursor_type == 7) {
        /* Northwest-Southeast diagonal resize cursor (NWSE) */
        const int cx = x - tb_disp_cursor_scale(size, 16);
        const int cy = y - tb_disp_cursor_scale(size, 16);
        SDL_Point shadow[] = {
            tb_disp_cursor_point(cx + 2, cy + 3, size, 4, 4),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 12, 4),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 10, 7),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 22, 19),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 28, 20),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 28, 28),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 20, 28),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 22, 25),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 10, 13),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 4, 12)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        tb_disp_fill_poly(d->ren, shadow, (int)(sizeof(shadow) / sizeof(shadow[0])));

        SDL_Point outline[] = {
            tb_disp_cursor_point(cx, cy, size, 4, 4),
            tb_disp_cursor_point(cx, cy, size, 12, 4),
            tb_disp_cursor_point(cx, cy, size, 10, 7),
            tb_disp_cursor_point(cx, cy, size, 22, 19),
            tb_disp_cursor_point(cx, cy, size, 28, 20),
            tb_disp_cursor_point(cx, cy, size, 28, 28),
            tb_disp_cursor_point(cx, cy, size, 20, 28),
            tb_disp_cursor_point(cx, cy, size, 22, 25),
            tb_disp_cursor_point(cx, cy, size, 10, 13),
            tb_disp_cursor_point(cx, cy, size, 4, 12)
        };
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        tb_disp_fill_poly(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));
        tb_disp_draw_poly_outline(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));

        SDL_Point body[] = {
            tb_disp_cursor_point(cx, cy, size, 7, 7),
            tb_disp_cursor_point(cx, cy, size, 12, 7),
            tb_disp_cursor_point(cx, cy, size, 10, 9),
            tb_disp_cursor_point(cx, cy, size, 21, 20),
            tb_disp_cursor_point(cx, cy, size, 25, 20),
            tb_disp_cursor_point(cx, cy, size, 25, 25),
            tb_disp_cursor_point(cx, cy, size, 20, 25),
            tb_disp_cursor_point(cx, cy, size, 22, 23),
            tb_disp_cursor_point(cx, cy, size, 11, 12),
            tb_disp_cursor_point(cx, cy, size, 7, 12)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        tb_disp_fill_poly(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
        tb_disp_draw_poly_outline(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
    }
    else if (d->cursor_type == 8) {
        /* Northeast-Southwest diagonal resize cursor (NESW) */
        const int cx = x - tb_disp_cursor_scale(size, 16);
        const int cy = y - tb_disp_cursor_scale(size, 16);
        SDL_Point shadow[] = {
            tb_disp_cursor_point(cx + 2, cy + 3, size, 28, 4),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 20, 4),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 22, 7),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 10, 19),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 4, 20),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 4, 28),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 12, 28),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 10, 25),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 22, 13),
            tb_disp_cursor_point(cx + 2, cy + 3, size, 28, 12)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        tb_disp_fill_poly(d->ren, shadow, (int)(sizeof(shadow) / sizeof(shadow[0])));

        SDL_Point outline[] = {
            tb_disp_cursor_point(cx, cy, size, 28, 4),
            tb_disp_cursor_point(cx, cy, size, 20, 4),
            tb_disp_cursor_point(cx, cy, size, 22, 7),
            tb_disp_cursor_point(cx, cy, size, 10, 19),
            tb_disp_cursor_point(cx, cy, size, 4, 20),
            tb_disp_cursor_point(cx, cy, size, 4, 28),
            tb_disp_cursor_point(cx, cy, size, 12, 28),
            tb_disp_cursor_point(cx, cy, size, 10, 25),
            tb_disp_cursor_point(cx, cy, size, 22, 13),
            tb_disp_cursor_point(cx, cy, size, 28, 12)
        };
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        tb_disp_fill_poly(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));
        tb_disp_draw_poly_outline(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));

        SDL_Point body[] = {
            tb_disp_cursor_point(cx, cy, size, 25, 7),
            tb_disp_cursor_point(cx, cy, size, 20, 7),
            tb_disp_cursor_point(cx, cy, size, 22, 9),
            tb_disp_cursor_point(cx, cy, size, 11, 20),
            tb_disp_cursor_point(cx, cy, size, 7, 20),
            tb_disp_cursor_point(cx, cy, size, 7, 25),
            tb_disp_cursor_point(cx, cy, size, 12, 25),
            tb_disp_cursor_point(cx, cy, size, 10, 23),
            tb_disp_cursor_point(cx, cy, size, 21, 12),
            tb_disp_cursor_point(cx, cy, size, 25, 12)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        tb_disp_fill_poly(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
        tb_disp_draw_poly_outline(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
    }
    else {
        /* Default: Arrow (Type 0, 5, or fallback) */
        SDL_Point shadow[] = {
            tb_disp_cursor_point(x + 2, y + 3, size, 0, 0),
            tb_disp_cursor_point(x + 2, y + 3, size, 0, 33),
            tb_disp_cursor_point(x + 2, y + 3, size, 7, 25),
            tb_disp_cursor_point(x + 2, y + 3, size, 13, 38),
            tb_disp_cursor_point(x + 2, y + 3, size, 20, 35),
            tb_disp_cursor_point(x + 2, y + 3, size, 14, 22),
            tb_disp_cursor_point(x + 2, y + 3, size, 24, 22)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 70);
        tb_disp_fill_poly(d->ren, shadow, (int)(sizeof(shadow) / sizeof(shadow[0])));

        SDL_Point outline[] = {
            tb_disp_cursor_point(x, y, size, 0, 0),
            tb_disp_cursor_point(x, y, size, 0, 33),
            tb_disp_cursor_point(x, y, size, 7, 25),
            tb_disp_cursor_point(x, y, size, 13, 38),
            tb_disp_cursor_point(x, y, size, 20, 35),
            tb_disp_cursor_point(x, y, size, 14, 22),
            tb_disp_cursor_point(x, y, size, 24, 22)
        };
        SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
        tb_disp_fill_poly(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));
        tb_disp_draw_poly_outline(d->ren, outline, (int)(sizeof(outline) / sizeof(outline[0])));

        SDL_Point body[] = {
            tb_disp_cursor_point(x, y, size, 3, 5),
            tb_disp_cursor_point(x, y, size, 3, 27),
            tb_disp_cursor_point(x, y, size, 8, 21),
            tb_disp_cursor_point(x, y, size, 14, 34),
            tb_disp_cursor_point(x, y, size, 16, 33),
            tb_disp_cursor_point(x, y, size, 11, 20),
            tb_disp_cursor_point(x, y, size, 19, 20)
        };
        SDL_SetRenderDrawColor(d->ren, 0, 0, 0, 255);
        tb_disp_fill_poly(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
        tb_disp_draw_poly_outline(d->ren, body, (int)(sizeof(body) / sizeof(body[0])));
    }

    SDL_SetRenderDrawBlendMode(d->ren, old_blend);
}

static void tb_disp_render_current(struct tb_display *d) {
    if (!d || !d->tex) return;
    SDL_RenderClear(d->ren);
    SDL_RenderCopy(d->ren, d->tex, NULL, NULL);
    tb_disp_draw_cursor(d);
    SDL_RenderPresent(d->ren);
}

void tb_disp_render_nv12(struct tb_display *d,
                         const uint8_t *y, int y_stride,
                         const uint8_t *uv, int uv_stride,
                         int w, int h) {
    if (tb_disp_ensure_texture(d, w, h) < 0) return;
    tb_disp_set_connection_state(d, 1);

    if (SDL_UpdateNVTexture(d->tex, NULL,
                            y,  y_stride,
                            uv, uv_stride) < 0) {
        fprintf(stderr, "[disp] UpdateNVTexture: %s\n", SDL_GetError());
        return;
    }
    d->last_video_frame_time = SDL_GetTicks();
    tb_disp_render_current(d);
}

void tb_disp_set_cursor(struct tb_display *d,
                        int x, int y,
                        int source_w, int source_h,
                        int visible,
                        int type,
                        int large) {
    if (!d) return;
    d->cursor_x = x;
    d->cursor_y = y;
    d->cursor_source_w = source_w > 0 ? source_w : 1;
    d->cursor_source_h = source_h > 0 ? source_h : 1;
    d->cursor_visible = visible;
    d->cursor_type = type;
    d->cursor_large = large;

    uint32_t now = SDL_GetTicks();
    if (now - d->last_video_frame_time > 40) {
        if (d->is_connected && d->tex) {
            tb_disp_render_current(d);
        }
    }
}

unsigned int tb_disp_poll_actions(struct tb_display *d) {
    unsigned int actions = TB_DISP_ACTION_NONE;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) d->quit = 1;
        else if (!d->input_capture_active &&
                 !d->input_intercept_active &&
                 ev.type == SDL_KEYDOWN &&
                 ev.key.keysym.sym == SDLK_ESCAPE) d->quit = 1;
        else if (!d->input_capture_active &&
                 !d->input_intercept_active &&
                 ev.type == SDL_KEYDOWN &&
                 ev.key.keysym.sym == SDLK_l) actions |= TB_DISP_ACTION_CYCLE_LANGUAGE;
        else if (d->input_capture_active) {
            struct tb_input_event input_event;
            memset(&input_event, 0, sizeof(input_event));
            switch (ev.type) {
            case SDL_MOUSEMOTION:
                if (ev.motion.x <= 2 && ev.motion.xrel < 0 && SDL_GetTicks() - d->last_target_switch_tick > 450) {
                    input_event.kind = TB_INPUT_EVENT_SWITCH_PREV_TARGET;
                    tb_disp_queue_input_event(d, &input_event);
                    d->last_target_switch_tick = SDL_GetTicks();
                    break;
                }
                if (ev.motion.x >= d->last_drawable_w - 2 && ev.motion.xrel > 0 && SDL_GetTicks() - d->last_target_switch_tick > 450) {
                    input_event.kind = TB_INPUT_EVENT_SWITCH_NEXT_TARGET;
                    tb_disp_queue_input_event(d, &input_event);
                    d->last_target_switch_tick = SDL_GetTicks();
                    break;
                }
                if (ev.motion.state & SDL_BUTTON_LMASK) input_event.kind = TB_INPUT_EVENT_LEFT_DRAG;
                else if (ev.motion.state & SDL_BUTTON_RMASK) input_event.kind = TB_INPUT_EVENT_RIGHT_DRAG;
                else if (ev.motion.state & SDL_BUTTON_MMASK) input_event.kind = TB_INPUT_EVENT_OTHER_DRAG;
                else input_event.kind = TB_INPUT_EVENT_MOVE;
                input_event.dx = ev.motion.xrel;
                input_event.dy = ev.motion.yrel;
                tb_disp_queue_input_event(d, &input_event);
                break;
            case SDL_MOUSEWHEEL:
            {
                uint32_t now = SDL_GetTicks();
                SDL_Keymod mods = SDL_GetModState();
                if ((mods & KMOD_ALT) &&
                    abs(ev.wheel.x) > abs(ev.wheel.y) &&
                    ev.wheel.x != 0 &&
                    now - d->last_space_switch_tick > 300) {
                    input_event.kind = ev.wheel.x > 0 ? TB_INPUT_EVENT_SWITCH_NEXT_SPACE : TB_INPUT_EVENT_SWITCH_PREV_SPACE;
                    tb_disp_queue_input_event(d, &input_event);
                    d->last_space_switch_tick = now;
                    d->space_gesture_accum_x = 0;
                    break;
                }
                if (abs(ev.wheel.x) > abs(ev.wheel.y) * 2 && ev.wheel.x != 0) {
                    if (now - d->last_space_gesture_tick > 250) {
                        d->space_gesture_accum_x = 0;
                    }
                    d->last_space_gesture_tick = now;
                    d->space_gesture_accum_x += ev.wheel.x;
                    if (abs(d->space_gesture_accum_x) >= 6 &&
                        now - d->last_space_switch_tick > 450) {
                        input_event.kind = d->space_gesture_accum_x > 0 ? TB_INPUT_EVENT_SWITCH_NEXT_SPACE : TB_INPUT_EVENT_SWITCH_PREV_SPACE;
                        tb_disp_queue_input_event(d, &input_event);
                        d->last_space_switch_tick = now;
                        d->space_gesture_accum_x = 0;
                    }
                    break;
                }
                if (now - d->last_space_gesture_tick > 250) {
                    d->space_gesture_accum_x = 0;
                }
                input_event.kind = TB_INPUT_EVENT_SCROLL;
                input_event.scroll_x = ev.wheel.x;
                input_event.scroll_y = ev.wheel.y;
                tb_disp_queue_input_event(d, &input_event);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) input_event.kind = TB_INPUT_EVENT_LEFT_DOWN;
                else if (ev.button.button == SDL_BUTTON_RIGHT) input_event.kind = TB_INPUT_EVENT_RIGHT_DOWN;
                else input_event.kind = TB_INPUT_EVENT_OTHER_DOWN;
                input_event.click_count = ev.button.clicks;
                tb_disp_queue_input_event(d, &input_event);
                break;
            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) input_event.kind = TB_INPUT_EVENT_LEFT_UP;
                else if (ev.button.button == SDL_BUTTON_RIGHT) input_event.kind = TB_INPUT_EVENT_RIGHT_UP;
                else input_event.kind = TB_INPUT_EVENT_OTHER_UP;
                input_event.click_count = ev.button.clicks;
                tb_disp_queue_input_event(d, &input_event);
                break;
            case SDL_KEYDOWN:
                if (!ev.key.repeat) {
                    if ((ev.key.keysym.mod & KMOD_CTRL) &&
                        (ev.key.keysym.mod & KMOD_ALT) &&
                        (ev.key.keysym.mod & KMOD_GUI) &&
                        ev.key.keysym.sym == SDLK_k) {
                        input_event.kind = TB_INPUT_EVENT_DEACTIVATE_CONTROL;
                        tb_disp_queue_input_event(d, &input_event);
                        break;
                    }
                    if ((ev.key.keysym.mod & KMOD_CTRL) && (ev.key.keysym.mod & KMOD_GUI)) {
                        if (ev.key.keysym.sym == SDLK_LEFT) {
                            input_event.kind = TB_INPUT_EVENT_SWITCH_PREV_TARGET;
                            tb_disp_queue_input_event(d, &input_event);
                            break;
                        }
                        if (ev.key.keysym.sym == SDLK_RIGHT) {
                            input_event.kind = TB_INPUT_EVENT_SWITCH_NEXT_TARGET;
                            tb_disp_queue_input_event(d, &input_event);
                            break;
                        }
                    }
                    uint16_t mac_key_code = tb_disp_mac_keycode_for_sdl_scancode(ev.key.keysym.scancode);
                    if (mac_key_code == 0xFFFF) break;
                    input_event.kind = TB_INPUT_EVENT_KEY_DOWN;
                    input_event.key_code = mac_key_code;
                    tb_disp_queue_input_event(d, &input_event);
                }
                break;
            case SDL_KEYUP:
                {
                if ((ev.key.keysym.mod & KMOD_CTRL) &&
                    (ev.key.keysym.mod & KMOD_ALT) &&
                    (ev.key.keysym.mod & KMOD_GUI) &&
                    ev.key.keysym.sym == SDLK_k) {
                    break;
                }
                if ((ev.key.keysym.mod & KMOD_CTRL) && (ev.key.keysym.mod & KMOD_GUI) &&
                    (ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_RIGHT)) {
                    break;
                }
                uint16_t mac_key_code = tb_disp_mac_keycode_for_sdl_scancode(ev.key.keysym.scancode);
                if (mac_key_code == 0xFFFF) break;
                input_event.kind = TB_INPUT_EVENT_KEY_UP;
                input_event.key_code = mac_key_code;
                tb_disp_queue_input_event(d, &input_event);
                }
                break;
            default:
                break;
            }
        }
    }
    if (d->quit) actions |= TB_DISP_ACTION_QUIT;
    return actions;
}

int tb_disp_pop_input_event(struct tb_display *d, struct tb_input_event *out) {
    if (!d || !out) return 0;
    return tb_input_queue_pop(&d->input_queue, out);
}

int tb_disp_get_info(struct tb_display *d, struct tb_display_info *info) {
    if (!d || !info || !d->win || !d->ren) return -1;

    memset(info, 0, sizeof(*info));

    int display_index = SDL_GetWindowDisplayIndex(d->win);
    if (display_index < 0) display_index = 0;

    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(display_index, &mode) == 0) {
        info->logical_w = (uint32_t)mode.w;
        info->logical_h = (uint32_t)mode.h;
        info->active_w = info->logical_w;
        info->active_h = info->logical_h;
    }

    int window_w = 0, window_h = 0;
    int drawable_w = 0, drawable_h = 0;
    SDL_GetWindowSize(d->win, &window_w, &window_h);
    if (SDL_GetRendererOutputSize(d->ren, &drawable_w, &drawable_h) == 0) {
        info->drawable_w = (uint32_t)drawable_w;
        info->drawable_h = (uint32_t)drawable_h;
    }

    /* SDL reports a logical desktop mode on Retina displays. Query the
     * receiver window's NSScreen for native pixels instead of using its
     * transient drawable, which is only window-sized before fullscreen. */
    uint32_t native_w = 0, native_h = 0;
    if (tb_receiver_content_display_pixels(&native_w, &native_h) == 0) {
        info->active_w = native_w;
        info->active_h = native_h;
    } else {
        if (info->drawable_w > info->active_w) info->active_w = info->drawable_w;
        if (info->drawable_h > info->active_h) info->active_h = info->drawable_h;
    }

    info->window_w = (uint32_t)window_w;
    info->window_h = (uint32_t)window_h;

    const char *name = SDL_GetDisplayName(display_index);
    if (!name) name = "Receiver Display";
    snprintf(info->name, sizeof(info->name), "%s", name);
    return 0;
}

static void tb_disp_set_stream_state(struct tb_display *d, int connected, int connecting) {
    if (!d) return;
    if (d->is_connected == connected && d->is_connecting == connecting) return;
    d->is_connected = connected;
    d->is_connecting = connecting;
    tb_disp_refresh_window_mode(d);
}

void tb_disp_set_connection_state(struct tb_display *d, int connected) {
    tb_disp_set_stream_state(d, connected ? 1 : 0, 0);
}

static void tb_disp_set_connecting_state(struct tb_display *d, int connecting) {
    tb_disp_set_stream_state(d, 0, connecting ? 1 : 0);
}

void tb_disp_set_input_capture_active(struct tb_display *d, int active) {
    if (!d) return;
    d->input_capture_active = active ? 1 : 0;
    tb_disp_refresh_window_mode(d);
}

void tb_disp_set_input_intercept_active(struct tb_display *d, int active) {
    if (!d) return;
    d->input_intercept_active = active ? 1 : 0;
    tb_disp_refresh_window_mode(d);
}

int tb_disp_window_on_active_space(struct tb_display *d) {
    /* Whether the receiver's display window is on the Space the user is
     * currently viewing. The receiverMaster global event tap fires for events
     * on every Space, but the user only intends to drive the sender while
     * looking at this (fullscreen) window. When they switch to another Space on
     * the receiver, this window is no longer on the active Space and the caller
     * stops forwarding so local work doesn't leak to the sender's cursor.
     *
     * Keyboard focus is not a reliable signal here: a fullscreen window can stay
     * key across a Space switch when the receiver app remains active on the new
     * (empty) Space. -[NSWindow isOnActiveSpace] is a direct, purely spatial
     * query to the window server, so it stays correct in that case. */
    if (!d || !d->win) return 1;
    return tb_window_on_active_space(d->win);
}

void tb_disp_render_status(struct tb_display *d,
                           const char *ip,
                           const char *address_label,
                           const char *status,
                           const char *sender,
                           const char *panel,
                           const char *mode,
                           const char *language,
                           const char *permissions) {
    if (!d || !d->ren || !d->win) return;

    tb_disp_set_connection_state(d, 0);

    if (!ip) ip = tb_i18n_get("receiver.network.not_detected");
    if (!address_label) address_label = tb_i18n_get("receiver.ui.ip_thunderbolt_bridge");
    if (!status) status = tb_i18n_get("receiver.status.waiting_for_sender");
    if (!sender) sender = tb_i18n_get("receiver.status.waiting");
    if (!panel) panel = tb_i18n_get("receiver.panel.unknown");
    if (!mode) mode = tb_i18n_get("receiver.mode.default");
    if (!language) language = tb_i18n_get("receiver.language.auto");
    if (!permissions) permissions = "";

    int drawable_w = 0, drawable_h = 0;
    if (SDL_GetRendererOutputSize(d->ren, &drawable_w, &drawable_h) < 0 ||
        drawable_w <= 0 || drawable_h <= 0) {
        drawable_w = 980;
        drawable_h = 620;
    }

    if (strcmp(d->last_ip, ip) != 0 ||
        strcmp(d->last_address_label, address_label) != 0 ||
        strcmp(d->last_status, status) != 0 ||
        strcmp(d->last_sender, sender) != 0 ||
        strcmp(d->last_panel, panel) != 0 ||
        strcmp(d->last_mode, mode) != 0 ||
        strcmp(d->last_language, language) != 0 ||
        strcmp(d->last_permissions, permissions) != 0 ||
        d->last_drawable_w != drawable_w ||
        d->last_drawable_h != drawable_h ||
        d->status_tex == NULL ||
        d->status_is_connecting) {
        snprintf(d->last_ip, sizeof(d->last_ip), "%s", ip);
        snprintf(d->last_address_label, sizeof(d->last_address_label), "%s", address_label);
        snprintf(d->last_status, sizeof(d->last_status), "%s", status);
        snprintf(d->last_sender, sizeof(d->last_sender), "%s", sender);
        snprintf(d->last_panel, sizeof(d->last_panel), "%s", panel);
        snprintf(d->last_mode, sizeof(d->last_mode), "%s", mode);
        snprintf(d->last_language, sizeof(d->last_language), "%s", language);
        snprintf(d->last_permissions, sizeof(d->last_permissions), "%s", permissions);
        d->last_drawable_w = drawable_w;
        d->last_drawable_h = drawable_h;
        d->status_is_connecting = 0;
        tb_disp_rebuild_status_texture(d, ip, address_label, status, sender, panel, mode, language, permissions,
                                       drawable_w, drawable_h, 0);
    }

    char title[256];
    snprintf(title, sizeof(title), "TBDisplayReceiverC %s — %s — %s", TB_RECEIVER_VERSION, ip, status);
    SDL_SetWindowTitle(d->win, title);

    SDL_RenderClear(d->ren);
    if (d->status_tex) SDL_RenderCopy(d->ren, d->status_tex, NULL, NULL);
    SDL_RenderPresent(d->ren);
}

void tb_disp_render_connecting(struct tb_display *d) {
    if (!d || !d->ren || !d->win) return;

    tb_disp_set_connecting_state(d, 1);

    int drawable_w = 0;
    int drawable_h = 0;
    if (SDL_GetRendererOutputSize(d->ren, &drawable_w, &drawable_h) < 0 ||
        drawable_w <= 0 || drawable_h <= 0) {
        drawable_w = 980;
        drawable_h = 620;
    }

    if (d->status_tex == NULL ||
        !d->status_is_connecting ||
        d->last_drawable_w != drawable_w ||
        d->last_drawable_h != drawable_h) {
        d->last_drawable_w = drawable_w;
        d->last_drawable_h = drawable_h;
        d->status_is_connecting = 1;
        tb_disp_rebuild_status_texture(d, "", "", "", "", "", "", "", "",
                                       drawable_w, drawable_h, 1);
    }

    SDL_SetWindowTitle(d->win, "TargetBridge Receiver — Connecting");
    SDL_RenderClear(d->ren);
    if (d->status_tex) SDL_RenderCopy(d->ren, d->status_tex, NULL, NULL);
    tb_disp_draw_connecting_spinner(d, drawable_w, drawable_h);
    SDL_RenderPresent(d->ren);
}

void tb_disp_set_brightness(struct tb_display *d, double level) {
    (void)d;
    if (level < 0.0) level = 0.0;
    if (level > 1.0) level = 1.0;

    void *lib = dlopen("/System/Library/PrivateFrameworks/DisplayServices.framework/Versions/A/DisplayServices", RTLD_LAZY);
    if (!lib) {
        fprintf(stderr, "[disp] failed to dlopen DisplayServices\n");
        return;
    }

    typedef int (*SetBrightnessFunc)(CGDirectDisplayID display, float brightness);
    SetBrightnessFunc set_brightness = (SetBrightnessFunc)dlsym(lib, "DisplayServicesSetBrightness");
    if (!set_brightness) {
        fprintf(stderr, "[disp] failed to find DisplayServicesSetBrightness symbol\n");
        dlclose(lib);
        return;
    }

    CGDirectDisplayID displays[16];
    uint32_t count = 0;
    if (CGGetActiveDisplayList(16, displays, &count) == kCGErrorSuccess) {
        for (uint32_t i = 0; i < count; i++) {
            set_brightness(displays[i], (float)level);
        }
    } else {
        set_brightness(CGMainDisplayID(), (float)level);
    }

    dlclose(lib);
}
