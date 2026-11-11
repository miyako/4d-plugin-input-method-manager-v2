/*
 Platform interface. imm-win.cpp implements it with IMM32 (imm.h) and the Text Services
 Framework (msctf.h); imm-mac.mm with Text Input Sources (Carbon) and NSTextInputContext.

 Every function except phonetic() touches the user interface: the dispatcher calls it
 through PA_RunInMainProcess, so it always runs on the thread that owns 4D's windows.
*/

#ifndef IMM_BACKEND_H
#define IMM_BACKEND_H

#include <cstdint>
#include <string>
#include <vector>

namespace imm {

// Status codes returned by the commands added in v2 (constants theme "IM Error").
enum Status : int32_t {
    ok = 0,
    no_window = -1,       // no window or text field has keyboard focus
    no_input_method = -2, // no input method (or input context) for the focused window
    not_supported = -3,   // not available on this platform or system
    invalid_argument = -4,
    not_found = -5,       // no input method with this identifier
    failed = -6           // the system or the input method rejected the request
};

// IM input mode constants (theme "IM Input Mode").
enum InputMode : int32_t {
    mode_unknown = -1,
    mode_direct_input = 0,
    mode_hiragana = 1,
    mode_katakana = 2,
    mode_halfwidth_katakana = 3,
    mode_fullwidth_alphanumeric = 4,
    mode_halfwidth_alphanumeric = 5
};

// IME conversion and sentence mode bits; identical to IME_CMODE_* / TF_CONVERSIONMODE_*.
enum : int32_t {
    cmode_direct_input = -1,
    cmode_native = 0x0001,
    cmode_katakana = 0x0002,
    cmode_fullshape = 0x0008,
    cmode_roman = 0x0010
};

struct Composition {
    bool active = false;
    std::u16string text;     // composition (marked) text
    std::u16string reading;  // reading of the composition, when the input method gives one
    int32_t cursor = 0;      // caret position in text, in UTF-16 units
};

struct InputMethod {
    std::u16string id;
    std::u16string name;
    std::u16string language;  // BCP 47 tag, e.g. "ja-JP" or "ja"
    std::string type;         // "keyboard", "ime" or "mode"
    bool selected = false;
};

void init();
void deinit();

// --- legacy commands (return 1 on success, 0 on failure, like the v1 plugin) ---
int32_t associate(bool enable);
int32_t get_mode(int32_t &conversion, int32_t &sentence);
int32_t set_mode(int32_t conversion, int32_t sentence);

// --- v2 commands (return a Status unless stated otherwise) ---
int32_t get_open_status();               // 1 open, 0 closed, or a negative Status
int32_t set_open_status(bool open);
int32_t get_input_mode();                // InputMode, or mode_unknown
int32_t set_input_mode(int32_t mode);
int32_t get_composition(Composition &composition);
int32_t end_composition(bool commit);
int32_t list_input_methods(std::vector<InputMethod> &methods);
int32_t current_input_method(InputMethod &method);
int32_t select_input_method(const std::u16string &id);
int32_t set_reading_capture(bool enable);
int32_t get_reading(std::u16string &reading, bool clear);

// Thread-safe: may run in a preemptive process. Result is hiragana.
int32_t phonetic(const std::u16string &text, std::u16string &reading);

}

#endif
