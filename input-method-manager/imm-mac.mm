/*
 macOS backend: Text Input Sources (Carbon HIToolbox) and NSTextInputContext.

 macOS has no per-window IME conversion mode. Each mode of an input method is its own input
 source (for the Apple Japanese input method: com.apple.inputmethod.Japanese, ...Katakana,
 ...HalfWidthKana, ...FullWidthRoman and com.apple.inputmethod.Roman), so the commands select
 the input source whose input mode identifier matches, preferring the input method that is
 already selected. Google Japanese Input and ATOK use the same input mode identifiers.

 Compiled with ARC.
*/

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "imm-backend.h"
#include "kana.h"

namespace {

NSString *const kModeHiragana = @"com.apple.inputmethod.Japanese";
NSString *const kModeKatakana = @"com.apple.inputmethod.Japanese.Katakana";
NSString *const kModeHalfwidthKana = @"com.apple.inputmethod.Japanese.HalfWidthKana";
NSString *const kModeFullwidthRoman = @"com.apple.inputmethod.Japanese.FullWidthRoman";
NSString *const kModeRoman = @"com.apple.inputmethod.Roman";

NSString *to_ns(const std::u16string &s) {
    return [NSString stringWithCharacters:(const unichar *)s.data() length:s.size()];
}

std::u16string to_u16(NSString *s) {
    if (!s) return std::u16string();
    std::u16string out(size_t(s.length), u'\0');
    if (!out.empty()) [s getCharacters:(unichar *)&out[0] range:NSMakeRange(0, s.length)];
    return out;
}

id property(TISInputSourceRef source, CFStringRef key) {
    return source ? (__bridge id)TISGetInputSourceProperty(source, key) : nil;
}

NSString *string_property(TISInputSourceRef source, CFStringRef key) {
    id value = property(source, key);
    return [value isKindOfClass:[NSString class]] ? (NSString *)value : nil;
}

bool bool_property(TISInputSourceRef source, CFStringRef key) {
    id value = property(source, key);
    return [value isKindOfClass:[NSNumber class]] && [(NSNumber *)value boolValue];
}

// TIS and AppKit text input belong to the main thread.
bool on_main_thread() { return [NSThread isMainThread]; }

NSArray *selectable_sources() {
    NSDictionary *filter = @{
        (__bridge id)kTISPropertyInputSourceCategory: (__bridge id)kTISCategoryKeyboardInputSource,
        (__bridge id)kTISPropertyInputSourceIsSelectCapable: @YES
    };
    return CFBridgingRelease(TISCreateInputSourceList((__bridge CFDictionaryRef)filter, false));
}

id current_source() { return CFBridgingRelease(TISCopyCurrentKeyboardInputSource()); }

bool is_open(TISInputSourceRef source) {
    return source && !bool_property(source, kTISPropertyInputSourceIsASCIICapable);
}

// Input source with this input mode identifier, preferring the input method `bundle`.
id find_mode(NSString *mode, NSString *bundle) {
    id fallback = nil;
    for (id item in selectable_sources()) {
        TISInputSourceRef source = (__bridge TISInputSourceRef)item;
        if (![string_property(source, kTISPropertyInputModeID) isEqualToString:mode]) continue;
        if (bundle && [string_property(source, kTISPropertyBundleID) isEqualToString:bundle]) return item;
        if (!fallback) fallback = item;
    }
    return fallback;
}

int32_t select(id item) {
    if (!item) return imm::no_input_method;
    return TISSelectInputSource((__bridge TISInputSourceRef)item) == noErr ? imm::ok : imm::failed;
}

int32_t select_direct_input() {
    id current = current_source();
    if (!is_open((__bridge TISInputSourceRef)current)) return imm::ok;
    // The Roman mode of the same input method, as the Eisu key selects; otherwise the
    // ASCII-capable source the user used last.
    NSString *bundle = string_property((__bridge TISInputSourceRef)current, kTISPropertyBundleID);
    id roman = find_mode(kModeRoman, bundle);
    if (roman && [string_property((__bridge TISInputSourceRef)roman, kTISPropertyBundleID) isEqualToString:bundle])
        return select(roman);
    return select(CFBridgingRelease(TISCopyCurrentASCIICapableKeyboardInputSource()));
}

int32_t mode_from_source(TISInputSourceRef source) {
    if (!source) return imm::mode_unknown;
    if (!is_open(source)) return imm::mode_direct_input;
    NSString *mode = string_property(source, kTISPropertyInputModeID);
    if ([mode isEqualToString:kModeHiragana]) return imm::mode_hiragana;
    if ([mode isEqualToString:kModeKatakana]) return imm::mode_katakana;
    if ([mode isEqualToString:kModeHalfwidthKana]) return imm::mode_halfwidth_katakana;
    if ([mode isEqualToString:kModeFullwidthRoman]) return imm::mode_fullwidth_alphanumeric;
    return imm::mode_unknown;
}

int32_t select_mode(int32_t mode) {
    NSString *identifier = nil;
    switch (mode) {
        case imm::mode_direct_input:
        case imm::mode_halfwidth_alphanumeric:
            return select_direct_input();
        case imm::mode_hiragana: identifier = kModeHiragana; break;
        case imm::mode_katakana: identifier = kModeKatakana; break;
        case imm::mode_halfwidth_katakana: identifier = kModeHalfwidthKana; break;
        case imm::mode_fullwidth_alphanumeric: identifier = kModeFullwidthRoman; break;
        default: return imm::invalid_argument;
    }
    id current = current_source();
    return select(find_mode(identifier, string_property((__bridge TISInputSourceRef)current, kTISPropertyBundleID)));
}

void fill(imm::InputMethod &m, TISInputSourceRef source) {
    m.id = to_u16(string_property(source, kTISPropertyInputSourceID));
    m.name = to_u16(string_property(source, kTISPropertyLocalizedName));
    id languages = property(source, kTISPropertyInputSourceLanguages);
    if ([languages isKindOfClass:[NSArray class]] && [(NSArray *)languages count])
        m.language = to_u16([(NSArray *)languages firstObject]);
    NSString *type = string_property(source, kTISPropertyInputSourceType);
    if ([type isEqualToString:(__bridge NSString *)kTISTypeKeyboardLayout]) m.type = "keyboard";
    else if ([type isEqualToString:(__bridge NSString *)kTISTypeKeyboardInputMode]) m.type = "mode";
    else m.type = "ime";
    m.selected = bool_property(source, kTISPropertyInputSourceIsSelected);
}

NSTextInputContext *input_context() { return [NSTextInputContext currentInputContext]; }

}

namespace imm {

void init() {}
void deinit() {}

int32_t associate(bool enable) {
    if (!on_main_thread()) return 0;
    @autoreleasepool {
        NSTextInputContext *context = input_context();
        if (!context) return 0;
        // A field limited to Roman input sources is the macOS equivalent of a window
        // without an input context.
        context.allowedInputSourceLocales = enable ? nil : @[NSAllRomanInputSourcesLocaleIdentifier];
        if (!enable) select_direct_input();
        return 1;
    }
}

int32_t get_mode(int32_t &conversion, int32_t &sentence) {
    if (!on_main_thread()) return 0;
    @autoreleasepool {
        id current = current_source();
        if (!current) return 0;
        TISInputSourceRef source = (__bridge TISInputSourceRef)current;
        sentence = 0;
        int32_t roman = [string_property(source, kTISPropertyInputSourceID) containsString:@"KanaTyping"] ? 0 : cmode_roman;
        switch (mode_from_source(source)) {
            case mode_direct_input: conversion = cmode_direct_input; break;
            case mode_hiragana: conversion = cmode_native | cmode_fullshape | roman; break;
            case mode_katakana: conversion = cmode_native | cmode_katakana | cmode_fullshape | roman; break;
            case mode_halfwidth_katakana: conversion = cmode_native | cmode_katakana | roman; break;
            case mode_fullwidth_alphanumeric: conversion = cmode_fullshape | roman; break;
            default: conversion = cmode_native; break;  // another language's input method
        }
        return 1;
    }
}

int32_t set_mode(int32_t conversion, int32_t /* sentence: no macOS equivalent */) {
    int32_t mode;
    if (conversion == cmode_direct_input) mode = mode_direct_input;
    else if (conversion & cmode_native)
        mode = (conversion & cmode_katakana)
                   ? ((conversion & cmode_fullshape) ? mode_katakana : mode_halfwidth_katakana)
                   : mode_hiragana;
    else mode = (conversion & cmode_fullshape) ? mode_fullwidth_alphanumeric : mode_halfwidth_alphanumeric;
    return set_input_mode(mode) == ok ? 1 : 0;
}

int32_t get_open_status() {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        id current = current_source();
        if (!current) return no_input_method;
        return is_open((__bridge TISInputSourceRef)current) ? 1 : 0;
    }
}

int32_t set_open_status(bool open) {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        if (!open) return select_direct_input();
        id current = current_source();
        if (is_open((__bridge TISInputSourceRef)current)) return ok;
        NSString *bundle = string_property((__bridge TISInputSourceRef)current, kTISPropertyBundleID);
        // The hiragana mode of the current (or any) Japanese input method, else any
        // non-ASCII input source of the current input method, else any non-ASCII source.
        id target = find_mode(kModeHiragana, bundle);
        if (!target) {
            id other = nil;
            for (id item in selectable_sources()) {
                TISInputSourceRef source = (__bridge TISInputSourceRef)item;
                if (!is_open(source)) continue;
                if (bundle && [string_property(source, kTISPropertyBundleID) isEqualToString:bundle]) { target = item; break; }
                if (!other) other = item;
            }
            if (!target) target = other;
        }
        return select(target);
    }
}

int32_t get_input_mode() {
    if (!on_main_thread()) return mode_unknown;
    @autoreleasepool {
        id current = current_source();
        return mode_from_source((__bridge TISInputSourceRef)current);
    }
}

int32_t set_input_mode(int32_t mode) {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        return select_mode(mode);
    }
}

int32_t get_composition(Composition &composition) {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        NSTextInputContext *context = input_context();
        id<NSTextInputClient> client = context.client;
        if (!client) return no_window;
        composition.active = [client hasMarkedText];
        if (!composition.active) return ok;
        NSRange marked = [client markedRange];
        if (marked.location == NSNotFound) {
            composition.active = false;
            return ok;
        }
        NSAttributedString *text = [client attributedSubstringForProposedRange:marked actualRange:NULL];
        composition.text = to_u16(text.string);
        NSRange selected = [client selectedRange];
        if (selected.location != NSNotFound && selected.location >= marked.location)
            composition.cursor = int32_t(MIN(selected.location - marked.location, marked.length));
        return ok;
    }
}

int32_t end_composition(bool commit) {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        NSTextInputContext *context = input_context();
        id<NSTextInputClient> client = context.client;
        if (!client) return no_window;
        if (![client hasMarkedText]) return ok;
        if (!commit) {
            [client setMarkedText:@"" selectedRange:NSMakeRange(0, 0) replacementRange:NSMakeRange(NSNotFound, 0)];
        }
        [client unmarkText];
        [context discardMarkedText];
        return ok;
    }
}

int32_t list_input_methods(std::vector<InputMethod> &methods) {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        for (id item in selectable_sources()) {
            InputMethod m;
            fill(m, (__bridge TISInputSourceRef)item);
            methods.push_back(m);
        }
        return ok;
    }
}

int32_t current_input_method(InputMethod &method) {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        id current = current_source();
        if (!current) return no_input_method;
        fill(method, (__bridge TISInputSourceRef)current);
        return ok;
    }
}

int32_t select_input_method(const std::u16string &source_id) {
    if (!on_main_thread()) return no_window;
    @autoreleasepool {
        NSString *identifier = to_ns(source_id);
        for (id item in selectable_sources()) {
            if ([string_property((__bridge TISInputSourceRef)item, kTISPropertyInputSourceID) isEqualToString:identifier])
                return select(item);
        }
        return not_found;
    }
}

// The reading typed into the input method is not available to applications on macOS.
int32_t set_reading_capture(bool) { return not_supported; }

int32_t get_reading(std::u16string &reading, bool) {
    reading.clear();
    return not_supported;
}

int32_t phonetic(const std::u16string &text, std::u16string &reading) {
    @autoreleasepool {
        reading.clear();
        NSString *source = to_ns(text);  // keeps the string alive for the CF calls below
        CFStringRef string = (__bridge CFStringRef)source;
        CFLocaleRef locale = CFLocaleCreate(kCFAllocatorDefault, CFSTR("ja_JP"));
        CFStringTokenizerRef tokenizer = CFStringTokenizerCreate(
            kCFAllocatorDefault, string, CFRangeMake(0, CFStringGetLength(string)),
            kCFStringTokenizerUnitWordBoundary, locale);
        if (locale) CFRelease(locale);
        if (!tokenizer) return failed;
        CFIndex position = 0;
        while (CFStringTokenizerAdvanceToNextToken(tokenizer) != kCFStringTokenizerTokenNone) {
            CFRange range = CFStringTokenizerGetCurrentTokenRange(tokenizer);
            if (range.location > position)  // spaces and punctuation between tokens
                reading += to_u16([source substringWithRange:NSMakeRange(position, range.location - position)]);
            std::u16string token = to_u16([source substringWithRange:NSMakeRange(range.location, range.length)]);
            position = range.location + range.length;
            if (!kana::has_kanji(token)) {  // kana, Latin, digits: keep the text itself
                reading += token;
                continue;
            }
            NSString *latin = CFBridgingRelease(CFStringTokenizerCopyCurrentTokenAttribute(
                tokenizer, kCFStringTokenizerAttributeLatinTranscription));
            if (!latin) {
                reading += token;
                continue;
            }
            NSMutableString *hiragana = [latin mutableCopy];
            CFStringTransform((__bridge CFMutableStringRef)hiragana, NULL, kCFStringTransformLatinHiragana, false);
            reading += to_u16(hiragana);
        }
        if (position < CFStringGetLength(string))
            reading += to_u16([source substringFromIndex:position]);
        CFRelease(tokenizer);
        return ok;
    }
}

}
