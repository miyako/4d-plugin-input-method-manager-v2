/*
 Input Method Manager v2: control of the input method (IME / FEP) of the focused 4D window.

 The dispatcher only converts parameters and results. Platform work is in imm-win.cpp
 (IMM32 + Text Services Framework) and imm-mac.mm (Text Input Sources + NSTextInputContext).
 Commands that touch the user interface run their platform code through PA_RunInMainProcess.
*/

#include "input-method-manager-4dplugin.h"
#include "imm-backend.h"
#include "kana.h"

#include <cstring>
#include <string>
#include <vector>

// --- helpers -----------------------------------------------------------------------------

namespace {

std::u16string get_text(PA_PluginParameters params, short index) {
    PA_Unistring *u = PA_GetStringParameter(params, index);
    if (!u || !u->fString || u->fLength <= 0) return std::u16string();
    return std::u16string((const char16_t *)u->fString, size_t(u->fLength));
}

void return_text(PA_PluginParameters params, const std::u16string &s) {
    std::u16string copy(s);  // c_str() is null-terminated
    PA_ReturnString(params, (PA_Unichar *)copy.c_str());
}

std::u16string ascii16(const std::string &s) {
    return std::u16string(s.begin(), s.end());
}

void ob_set_var(PA_ObjectRef o, const char *key, PA_Variable *v) {
    std::u16string k = ascii16(key);
    PA_Unistring uk = PA_CreateUnistring((PA_Unichar *)k.c_str());
    PA_SetObjectProperty(o, &uk, *v);  // the runtime copies the value
    PA_DisposeUnistring(&uk);
    PA_ClearVariable(v);
}

void ob_set_s(PA_ObjectRef o, const char *key, const std::u16string &value) {
    std::u16string copy(value);
    PA_Variable v = PA_CreateVariable(eVK_Unistring);
    PA_Unistring us = PA_CreateUnistring((PA_Unichar *)copy.c_str());
    PA_SetStringVariable(&v, &us);
    ob_set_var(o, key, &v);
}

void ob_set_n(PA_ObjectRef o, const char *key, PA_long32 value) {
    PA_Variable v = PA_CreateVariable(eVK_Longint);
    PA_SetLongintVariable(&v, value);
    ob_set_var(o, key, &v);
}

void ob_set_b(PA_ObjectRef o, const char *key, bool value) {
    PA_Variable v = PA_CreateVariable(eVK_Boolean);
    PA_SetBooleanVariable(&v, value ? 1 : 0);
    ob_set_var(o, key, &v);
}

PA_ObjectRef input_method_object(const imm::InputMethod &m) {
    PA_ObjectRef o = PA_CreateObject();
    ob_set_s(o, "id", m.id);
    ob_set_s(o, "name", m.name);
    ob_set_s(o, "language", m.language);
    ob_set_s(o, "type", ascii16(m.type));
    ob_set_b(o, "selected", m.selected);
    return o;
}

// Runs f on the thread that owns 4D's windows. PA_RunInMainProcess returns after f returns.
template <typename F>
void run_in_main(F &&f) {
    struct Context {
        F *f;
    } context{&f};
    PA_RunInMainProcess(
        [](void *p) {
            try {
                (*static_cast<Context *>(p)->f)();
            } catch (...) {
            }
        },
        &context);
}

}

// --- dispatcher --------------------------------------------------------------------------

void PluginMain(PA_long32 selector, PA_PluginParameters params) {
    try {
        switch (selector) {
            case kInitPlugin:
            case kServerInitPlugin:
                imm::init();
                break;
            case kDeinitPlugin:
            case kServerDeinitPlugin:
                imm::deinit();
                break;

            case 1: IM_Associate(params); break;
            case 2: IM_Disassociate(params); break;
            case 3: IM_Get_mode(params); break;
            case 4: IM_Set_mode(params); break;
            case 5: IM_Get_open_status(params); break;
            case 6: IM_Set_open_status(params); break;
            case 7: IM_Get_input_mode(params); break;
            case 8: IM_Set_input_mode(params); break;
            case 9: IM_Get_composition(params); break;
            case 10: IM_Complete_composition(params); break;
            case 11: IM_Cancel_composition(params); break;
            case 12: IM_Get_input_methods(params); break;
            case 13: IM_Get_input_method(params); break;
            case 14: IM_Set_input_method(params); break;
            case 15: IM_Set_reading_capture(params); break;
            case 16: IM_Get_reading(params); break;
            case 17: IM_Get_phonetic(params); break;
            case 18: IM_Convert_kana(params); break;
        }
    } catch (...) {
    }
}

// --- Imm Context -------------------------------------------------------------------------

static void IM_Associate(PA_PluginParameters params) {
    int32_t result = 0;
    run_in_main([&] { result = imm::associate(true); });
    PA_ReturnLong(params, result);
}

static void IM_Disassociate(PA_PluginParameters params) {
    int32_t result = 0;
    run_in_main([&] { result = imm::associate(false); });
    PA_ReturnLong(params, result);
}

// --- Imm Status --------------------------------------------------------------------------

static void IM_Get_mode(PA_PluginParameters params) {
    int32_t result = 0, conversion = 0, sentence = 0;
    run_in_main([&] { result = imm::get_mode(conversion, sentence); });
    if (result) {
        PA_SetLongParameter(params, 1, conversion);
        PA_SetLongParameter(params, 2, sentence);
    }
    PA_ReturnLong(params, result);
}

static void IM_Set_mode(PA_PluginParameters params) {
    int32_t conversion = PA_GetLongParameter(params, 1);
    int32_t sentence = PA_GetLongParameter(params, 2);
    int32_t result = 0;
    run_in_main([&] { result = imm::set_mode(conversion, sentence); });
    PA_ReturnLong(params, result);
}

static void IM_Get_open_status(PA_PluginParameters params) {
    int32_t result = imm::no_window;
    run_in_main([&] { result = imm::get_open_status(); });
    PA_ReturnLong(params, result);
}

static void IM_Set_open_status(PA_PluginParameters params) {
    bool open = PA_GetLongParameter(params, 1) != 0;
    int32_t result = imm::no_window;
    run_in_main([&] { result = imm::set_open_status(open); });
    PA_ReturnLong(params, result);
}

static void IM_Get_input_mode(PA_PluginParameters params) {
    int32_t result = imm::mode_unknown;
    run_in_main([&] { result = imm::get_input_mode(); });
    PA_ReturnLong(params, result);
}

static void IM_Set_input_mode(PA_PluginParameters params) {
    int32_t mode = PA_GetLongParameter(params, 1);
    int32_t result = imm::invalid_argument;
    if (mode >= imm::mode_direct_input && mode <= imm::mode_halfwidth_alphanumeric) {
        run_in_main([&] { result = imm::set_input_mode(mode); });
    }
    PA_ReturnLong(params, result);
}

// --- Imm Composition ---------------------------------------------------------------------

static void IM_Get_composition(PA_PluginParameters params) {
    imm::Composition composition;
    int32_t status = imm::no_window;
    run_in_main([&] { status = imm::get_composition(composition); });
    PA_ObjectRef o = PA_CreateObject();
    ob_set_n(o, "status", status);
    ob_set_b(o, "active", status == imm::ok && composition.active);
    ob_set_s(o, "text", composition.text);
    ob_set_s(o, "reading", composition.reading);
    ob_set_n(o, "cursor", composition.cursor);
    PA_ReturnObject(params, o);
}

static void IM_Complete_composition(PA_PluginParameters params) {
    int32_t result = imm::no_window;
    run_in_main([&] { result = imm::end_composition(true); });
    PA_ReturnLong(params, result);
}

static void IM_Cancel_composition(PA_PluginParameters params) {
    int32_t result = imm::no_window;
    run_in_main([&] { result = imm::end_composition(false); });
    PA_ReturnLong(params, result);
}

// --- Imm Input Method --------------------------------------------------------------------

static void IM_Get_input_methods(PA_PluginParameters params) {
    std::vector<imm::InputMethod> methods;
    run_in_main([&] { imm::list_input_methods(methods); });
    PA_CollectionRef c = PA_CreateCollection();
    PA_long32 i = 0;
    for (const imm::InputMethod &m : methods) {
        PA_Variable v = PA_CreateVariable(eVK_Object);
        PA_SetObjectVariable(&v, input_method_object(m));
        PA_SetCollectionElement(c, i++, v);  // the runtime copies the element
        PA_ClearVariable(&v);
    }
    PA_ReturnCollection(params, c);
}

static void IM_Get_input_method(PA_PluginParameters params) {
    imm::InputMethod method;
    int32_t status = imm::not_supported;
    run_in_main([&] { status = imm::current_input_method(method); });
    PA_ObjectRef o = input_method_object(method);
    ob_set_n(o, "status", status);
    PA_ReturnObject(params, o);
}

static void IM_Set_input_method(PA_PluginParameters params) {
    std::u16string id = get_text(params, 1);
    int32_t result = imm::invalid_argument;
    if (!id.empty()) {
        run_in_main([&] { result = imm::select_input_method(id); });
    }
    PA_ReturnLong(params, result);
}

// --- Imm Reading -------------------------------------------------------------------------

static void IM_Set_reading_capture(PA_PluginParameters params) {
    bool enable = PA_GetLongParameter(params, 1) != 0;
    int32_t result = imm::not_supported;
    run_in_main([&] { result = imm::set_reading_capture(enable); });
    PA_ReturnLong(params, result);
}

static void IM_Get_reading(PA_PluginParameters params) {
    PA_long32 form = PA_GetLongParameter(params, 1);
    std::u16string reading;
    if (kana::is_valid_form(form)) {
        run_in_main([&] { imm::get_reading(reading, false); });
        reading = kana::convert(reading, kana::Form(form));
    }
    return_text(params, reading);
}

static void IM_Get_phonetic(PA_PluginParameters params) {
    std::u16string text = get_text(params, 1);
    PA_long32 form = PA_GetLongParameter(params, 2);
    std::u16string reading;
    if (kana::is_valid_form(form) && !text.empty() && imm::phonetic(text, reading) == imm::ok) {
        reading = kana::convert(reading, kana::Form(form));
    } else {
        reading.clear();
    }
    return_text(params, reading);
}

static void IM_Convert_kana(PA_PluginParameters params) {
    std::u16string text = get_text(params, 1);
    PA_long32 form = PA_GetLongParameter(params, 2);
    return_text(params, kana::is_valid_form(form) ? kana::convert(text, kana::Form(form)) : std::u16string());
}
