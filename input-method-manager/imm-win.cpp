/*
 Windows backend.

 - IMM32 (imm.h): the input context of the focused window (associate/disassociate),
   composition strings, and the WM_IME_COMPOSITION messages used to capture readings.
 - Text Services Framework (msctf.h): when the active keyboard profile is a text input
   processor (the Microsoft IME and every current IME is), the open/close and conversion
   state is read and written through the thread manager's compartments
   (GUID_COMPARTMENT_KEYBOARD_*), with IMM32 as the fallback. Input method profiles are
   listed and activated with ITfInputProcessorProfileMgr.
 - IFELanguage (MSIME.Japan): the phonetic reading of Japanese text.

 Everything except phonetic() runs on the thread that owns 4D's windows (the dispatcher
 uses PA_RunInMainProcess), so GetFocus() and ImmGetContext() see 4D's windows.
*/

#include <windows.h>
#include <imm.h>
#include <msctf.h>
#include <oleauto.h>
#include <wrl/client.h>

#include <cstdio>
#include <cwchar>
#include <memory>
#include <mutex>

#include "imm-backend.h"
#include "kana.h"

using Microsoft::WRL::ComPtr;

namespace {

// --- COM ---------------------------------------------------------------------------------

struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope() {
        if (SUCCEEDED(hr)) CoUninitialize();  // S_OK and S_FALSE both need balancing
    }
};

std::u16string from_wide(const wchar_t *s, size_t n) { return std::u16string((const char16_t *)s, n); }
std::u16string from_wide(const wchar_t *s) { return s ? from_wide(s, wcslen(s)) : std::u16string(); }
std::wstring to_wide(const std::u16string &s) { return std::wstring((const wchar_t *)s.data(), s.size()); }

std::u16string from_bstr(BSTR b) { return b ? from_wide(b, SysStringLen(b)) : std::u16string(); }

// --- window and input context ------------------------------------------------------------

HWND focus_window() {
    HWND hwnd = GetFocus();
    if (!hwnd) {
        HWND foreground = GetForegroundWindow();
        if (foreground && GetWindowThreadProcessId(foreground, nullptr) == GetCurrentThreadId()) hwnd = foreground;
    }
    return hwnd;
}

class Context {
public:
    explicit Context(HWND hwnd) : hwnd_(hwnd), himc_(hwnd ? ImmGetContext(hwnd) : nullptr) {}
    ~Context() {
        if (himc_) ImmReleaseContext(hwnd_, himc_);
    }
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    HIMC get() const { return himc_; }

private:
    HWND hwnd_;
    HIMC himc_;
};

std::u16string composition_string(HIMC himc, DWORD index) {
    LONG bytes = ImmGetCompositionStringW(himc, index, nullptr, 0);
    if (bytes <= 0) return std::u16string();
    std::wstring buffer(size_t(bytes) / sizeof(wchar_t), L'\0');
    bytes = ImmGetCompositionStringW(himc, index, &buffer[0], DWORD(buffer.size() * sizeof(wchar_t)));
    if (bytes <= 0) return std::u16string();
    return from_wide(buffer.data(), size_t(bytes) / sizeof(wchar_t));
}

// --- Text Services Framework -------------------------------------------------------------

bool active_profile(TF_INPUTPROCESSORPROFILE &profile) {
    ComPtr<ITfInputProcessorProfileMgr> manager;
    if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager))))
        return false;
    ZeroMemory(&profile, sizeof(profile));
    return manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile) == S_OK;
}

// The keyboard compartments of this thread's TSF thread manager.
class Compartments {
public:
    Compartments() {
        if (FAILED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&thread_)))) return;
        if (SUCCEEDED(thread_->Activate(&client_))) activated_ = true;  // reference counted
        thread_.As(&manager_);
    }
    ~Compartments() {
        if (activated_) thread_->Deactivate();
    }
    Compartments(const Compartments &) = delete;
    Compartments &operator=(const Compartments &) = delete;

    bool valid() const { return activated_ && manager_; }

    bool get(REFGUID guid, DWORD &value) {
        ComPtr<ITfCompartment> compartment;
        if (!manager_ || FAILED(manager_->GetCompartment(guid, &compartment))) return false;
        VARIANT v;
        VariantInit(&v);
        if (compartment->GetValue(&v) != S_OK) return false;  // S_FALSE: never set
        bool found = v.vt == VT_I4;
        if (found) value = DWORD(v.lVal);
        VariantClear(&v);
        return found;
    }

    bool set(REFGUID guid, DWORD value) {
        ComPtr<ITfCompartment> compartment;
        if (!valid() || FAILED(manager_->GetCompartment(guid, &compartment))) return false;
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_I4;
        v.lVal = LONG(value);
        return SUCCEEDED(compartment->SetValue(client_, &v));
    }

private:
    ComPtr<ITfThreadMgr> thread_;
    ComPtr<ITfCompartmentMgr> manager_;
    TfClientId client_ = TF_CLIENTID_NULL;
    bool activated_ = false;
};

// Open/close and conversion state of the focused window: TSF compartments when the active
// profile is a text input processor, IMM32 otherwise or when a compartment call fails.
class Ime {
public:
    Ime() : hwnd_(focus_window()), context_(hwnd_) {
        if (!hwnd_) { status_ = imm::no_window; return; }
        if (!context_.get()) { status_ = imm::no_input_method; return; }  // disassociated
        TF_INPUTPROCESSORPROFILE profile;
        if (active_profile(profile) && profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR) {
            tsf_.reset(new Compartments());
            if (!tsf_->valid()) tsf_.reset();
        }
    }

    int32_t status() const { return status_; }
    HIMC himc() const { return context_.get(); }

    bool get_open(bool &open) {
        DWORD value = 0;
        if (tsf_ && tsf_->get(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, value)) { open = value != 0; return true; }
        open = ImmGetOpenStatus(himc()) != FALSE;
        return true;
    }

    bool set_open(bool open) {
        bool done = ImmSetOpenStatus(himc(), open ? TRUE : FALSE) != FALSE;
        if (tsf_) done = tsf_->set(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, open ? 1 : 0) || done;
        return done;
    }

    bool get_conversion(DWORD &conversion, DWORD &sentence) {
        if (tsf_ && tsf_->get(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, conversion)) {
            if (!tsf_->get(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE, sentence)) {
                DWORD c;
                if (!ImmGetConversionStatus(himc(), &c, &sentence)) sentence = 0;
            }
            return true;
        }
        return ImmGetConversionStatus(himc(), &conversion, &sentence) != FALSE;
    }

    bool set_conversion(DWORD conversion, DWORD sentence) {
        bool done = ImmSetConversionStatus(himc(), conversion, sentence) != FALSE;
        if (tsf_) {
            bool c = tsf_->set(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, conversion);
            bool s = tsf_->set(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE, sentence);
            done = (c && s) || done;
        }
        return done;
    }

private:
    ComScope com_;
    HWND hwnd_;
    Context context_;
    std::unique_ptr<Compartments> tsf_;
    int32_t status_ = imm::ok;
};

const DWORD kMask = imm::cmode_native | imm::cmode_katakana | imm::cmode_fullshape;

DWORD mode_bits(int32_t mode) {
    switch (mode) {
        case imm::mode_hiragana: return imm::cmode_native | imm::cmode_fullshape;
        case imm::mode_katakana: return imm::cmode_native | imm::cmode_katakana | imm::cmode_fullshape;
        case imm::mode_halfwidth_katakana: return imm::cmode_native | imm::cmode_katakana;
        case imm::mode_fullwidth_alphanumeric: return imm::cmode_fullshape;
        default: return 0;  // half-width alphanumeric
    }
}

// --- input method profiles ---------------------------------------------------------------

std::u16string language_tag(LANGID langid) {
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {0};
    if (LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) > 0) return from_wide(name);
    return std::u16string();
}

std::u16string guid_string(REFGUID guid) {
    wchar_t buffer[64] = {0};
    StringFromGUID2(guid, buffer, 64);
    return from_wide(buffer);
}

std::u16string hex(unsigned long value, int digits) {
    wchar_t buffer[16] = {0};
    swprintf(buffer, 16, L"%0*lX", digits, value);
    return from_wide(buffer);
}

// Display name of a keyboard layout from HKLM\SYSTEM\...\Keyboard Layouts\<KLID>.
std::u16string layout_name(HKL hkl, LANGID langid) {
    unsigned long value = (unsigned long)(ULONG_PTR)hkl;
    unsigned device = HIWORD(value);
    unsigned long klid = 0;
    bool variant = (device & 0xF000) == 0xF000;
    if ((device & 0xF000) == 0xE000) klid = value;  // IMM IME
    else if (!variant) klid = device ? device : langid;

    HKEY layouts;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts", 0, KEY_READ, &layouts) != ERROR_SUCCESS)
        return std::u16string();
    std::u16string name;
    wchar_t key[KL_NAMELENGTH + 8];
    for (DWORD index = 0; name.empty(); ++index) {
        DWORD size = KL_NAMELENGTH + 8;
        if (RegEnumKeyExW(layouts, index, key, &size, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        unsigned long id = wcstoul(key, nullptr, 16);
        HKEY layout;
        if (RegOpenKeyExW(layouts, key, 0, KEY_READ, &layout) != ERROR_SUCCESS) continue;
        bool match = false;
        if (variant) {
            wchar_t layout_id[16] = {0};
            DWORD bytes = sizeof(layout_id) - sizeof(wchar_t);
            if ((LOWORD(id) == langid || LOWORD(id) == LOWORD(value)) &&
                RegQueryValueExW(layout, L"Layout Id", nullptr, nullptr, (LPBYTE)layout_id, &bytes) == ERROR_SUCCESS)
                match = wcstoul(layout_id, nullptr, 16) == (device & 0x0FFF);
        } else {
            match = id == klid;
        }
        if (match) {
            wchar_t text[256] = {0};
            if (RegLoadMUIStringW(layout, L"Layout Display Name", text, sizeof(text), nullptr, 0, nullptr) == ERROR_SUCCESS) {
                name = from_wide(text);
            } else {
                DWORD bytes = sizeof(text) - sizeof(wchar_t);
                if (RegQueryValueExW(layout, L"Layout Text", nullptr, nullptr, (LPBYTE)text, &bytes) == ERROR_SUCCESS)
                    name = from_wide(text);
            }
        }
        RegCloseKey(layout);
    }
    RegCloseKey(layouts);
    return name;
}

// "tip:0411:{clsid}:{profile}" or "hkl:04110411".
std::u16string profile_id(const TF_INPUTPROCESSORPROFILE &p) {
    if (p.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR)
        return u"tip:" + hex(p.langid, 4) + u":" + guid_string(p.clsid) + u":" + guid_string(p.guidProfile);
    return u"hkl:" + hex((unsigned long)(ULONG_PTR)p.hkl, 8);
}

void fill(imm::InputMethod &m, const TF_INPUTPROCESSORPROFILE &p, ITfInputProcessorProfiles *profiles, bool selected) {
    m.id = profile_id(p);
    m.language = language_tag(p.langid);
    m.selected = selected;
    if (p.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR) {
        m.type = "ime";
        BSTR description = nullptr;
        if (profiles && SUCCEEDED(profiles->GetLanguageProfileDescription(p.clsid, p.langid, p.guidProfile, &description))) {
            m.name = from_bstr(description);
            SysFreeString(description);
        }
    } else {
        m.type = "keyboard";
        m.name = layout_name(p.hkl, p.langid);
    }
    if (m.name.empty()) {
        wchar_t display[256] = {0};
        std::wstring locale = to_wide(m.language);
        if (!locale.empty() && GetLocaleInfoEx(locale.c_str(), LOCALE_SLOCALIZEDDISPLAYNAME, display, 256) > 0)
            m.name = from_wide(display);
    }
}

bool same_profile(const TF_INPUTPROCESSORPROFILE &a, const TF_INPUTPROCESSORPROFILE &b) {
    if (a.dwProfileType != b.dwProfileType) return false;
    if (a.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR)
        return a.langid == b.langid && IsEqualGUID(a.clsid, b.clsid) && IsEqualGUID(a.guidProfile, b.guidProfile);
    return a.hkl == b.hkl;
}

template <typename F>
int32_t for_each_profile(F &&f) {
    ComPtr<ITfInputProcessorProfileMgr> manager;
    if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager))))
        return imm::not_supported;
    ComPtr<ITfInputProcessorProfiles> profiles;
    manager.As(&profiles);
    ComPtr<IEnumTfInputProcessorProfiles> enumerator;
    if (FAILED(manager->EnumProfiles(0, &enumerator)) || !enumerator) return imm::failed;
    TF_INPUTPROCESSORPROFILE p;
    ULONG fetched = 0;
    while (enumerator->Next(1, &p, &fetched) == S_OK && fetched == 1) {
        if (!(p.dwFlags & TF_IPP_FLAG_ENABLED)) continue;
        if (p.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR && !IsEqualGUID(p.catid, GUID_TFCAT_TIP_KEYBOARD)) continue;
        if (f(manager.Get(), profiles.Get(), p)) break;
    }
    return imm::ok;
}

// --- reading capture ---------------------------------------------------------------------

std::mutex reading_mutex;
HHOOK reading_hook = nullptr;
std::u16string reading_text;       // readings of the committed strings
std::u16string reading_candidate;  // reading of the composition in progress

void on_composition(HWND hwnd, LPARAM flags) {
    Context context(hwnd);
    HIMC himc = context.get();
    if (!himc) return;
    // Read everything from the input context before taking the lock.
    const bool has_result = (flags & GCS_RESULTSTR) != 0, has_composition = (flags & GCS_COMPSTR) != 0;
    std::u16string result, result_reading, composition, composition_reading;
    if (has_result) {
        result = composition_string(himc, GCS_RESULTSTR);
        if (flags & GCS_RESULTREADSTR) result_reading = composition_string(himc, GCS_RESULTREADSTR);
    }
    if (has_composition) {
        composition = composition_string(himc, GCS_COMPSTR);
        if (flags & GCS_COMPREADSTR) composition_reading = composition_string(himc, GCS_COMPREADSTR);
    }
    std::lock_guard<std::mutex> lock(reading_mutex);
    if (has_result) {
        std::u16string reading = result_reading;
        if (reading.empty()) {
            // Some IMEs (the current Microsoft IME among them) give no result reading: use
            // the kana typed before conversion, or the result itself when it has no kanji.
            if (!kana::has_kanji(result)) reading = result;
            else if (!reading_candidate.empty()) reading = reading_candidate;
            else reading = result;
        }
        reading_text += reading;
        reading_candidate.clear();
    }
    if (has_composition) {
        if (!composition_reading.empty()) reading_candidate = composition_reading;
        else if (kana::is_kana_only(composition)) reading_candidate = composition;
        else if (composition.empty()) reading_candidate.clear();
        // otherwise the composition has been converted: keep the kana typed before
    }
}

LRESULT CALLBACK call_window_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
        const CWPSTRUCT *message = reinterpret_cast<const CWPSTRUCT *>(lparam);
        if (message->message == WM_IME_STARTCOMPOSITION) {
            std::lock_guard<std::mutex> lock(reading_mutex);
            reading_candidate.clear();
        } else if (message->message == WM_IME_COMPOSITION) {
            on_composition(message->hwnd, message->lParam);
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

void remove_hook() {
    if (reading_hook) {
        UnhookWindowsHookEx(reading_hook);
        reading_hook = nullptr;
    }
}

// --- IFELanguage (msime.h is not part of the Windows SDK) ---------------------------------

struct __declspec(uuid("019F7152-E6DB-11d0-83C3-00C04FDDB82E")) IFELanguage : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Open() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetJMorphResult(DWORD request, DWORD mode, INT length, const WCHAR *input, DWORD *info, void **result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConversionModeCaps(DWORD *caps) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPhonetic(BSTR string, LONG start, LONG length, BSTR *result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConversion(BSTR string, LONG start, LONG length, BSTR *result) = 0;
};

}

namespace imm {

void init() {}

void deinit() {
    std::lock_guard<std::mutex> lock(reading_mutex);
    remove_hook();
}

// --- legacy ------------------------------------------------------------------------------

int32_t associate(bool enable) {
    HWND hwnd = focus_window();
    if (!hwnd) return 0;
    return ImmAssociateContextEx(hwnd, nullptr, enable ? IACE_DEFAULT : 0) ? 1 : 0;
}

int32_t get_mode(int32_t &conversion, int32_t &sentence) {
    Ime ime;
    if (ime.status() != ok) return 0;
    DWORD c = 0, s = 0;
    bool open = false;
    if (!ime.get_conversion(c, s)) return 0;
    ime.get_open(open);
    conversion = open ? int32_t(c) : cmode_direct_input;
    sentence = int32_t(s);
    return 1;
}

int32_t set_mode(int32_t conversion, int32_t sentence) {
    Ime ime;
    if (ime.status() != ok) return 0;
    if (conversion == cmode_direct_input) {
        DWORD c = 0, s = 0;
        if (ime.get_conversion(c, s)) ime.set_conversion(c, DWORD(sentence));
        return ime.set_open(false) ? 1 : 0;
    }
    if (!ime.set_open(true)) return 0;
    return ime.set_conversion(DWORD(conversion), DWORD(sentence)) ? 1 : 0;
}

// --- open status and input mode ----------------------------------------------------------

int32_t get_open_status() {
    Ime ime;
    if (ime.status() != ok) return ime.status();
    bool open = false;
    ime.get_open(open);
    return open ? 1 : 0;
}

int32_t set_open_status(bool open) {
    Ime ime;
    if (ime.status() != ok) return ime.status();
    return ime.set_open(open) ? ok : failed;
}

int32_t get_input_mode() {
    Ime ime;
    if (ime.status() != ok) return mode_unknown;
    bool open = false;
    DWORD c = 0, s = 0;
    ime.get_open(open);
    if (!open) return mode_direct_input;
    if (!ime.get_conversion(c, s)) return mode_unknown;
    if (c & cmode_native) {
        if (!(c & cmode_katakana)) return mode_hiragana;
        return (c & cmode_fullshape) ? mode_katakana : mode_halfwidth_katakana;
    }
    return (c & cmode_fullshape) ? mode_fullwidth_alphanumeric : mode_halfwidth_alphanumeric;
}

int32_t set_input_mode(int32_t mode) {
    Ime ime;
    if (ime.status() != ok) return ime.status();
    if (mode == mode_direct_input) return ime.set_open(false) ? ok : failed;
    DWORD c = 0, s = 0;
    if (!ime.get_conversion(c, s)) { c = cmode_roman; s = IME_SMODE_PHRASEPREDICT; }
    c = (c & ~kMask) | mode_bits(mode);  // keep unrelated bits such as romaji input
    if (!ime.set_open(true)) return failed;
    return ime.set_conversion(c, s) ? ok : failed;
}

// --- composition -------------------------------------------------------------------------

int32_t get_composition(Composition &composition) {
    HWND hwnd = focus_window();
    if (!hwnd) return no_window;
    Context context(hwnd);
    if (!context.get()) return no_input_method;
    composition.text = composition_string(context.get(), GCS_COMPSTR);
    composition.active = !composition.text.empty();
    if (composition.active) {
        composition.reading = composition_string(context.get(), GCS_COMPREADSTR);
        LONG cursor = ImmGetCompositionStringW(context.get(), GCS_CURSORPOS, nullptr, 0);
        composition.cursor = cursor > 0 ? int32_t(cursor) : 0;
    }
    return ok;
}

int32_t end_composition(bool commit) {
    HWND hwnd = focus_window();
    if (!hwnd) return no_window;
    Context context(hwnd);
    if (!context.get()) return no_input_method;
    if (composition_string(context.get(), GCS_COMPSTR).empty()) return ok;
    return ImmNotifyIME(context.get(), NI_COMPOSITIONSTR, commit ? CPS_COMPLETE : CPS_CANCEL, 0) ? ok : failed;
}

// --- input methods -----------------------------------------------------------------------

int32_t list_input_methods(std::vector<InputMethod> &methods) {
    ComScope com;
    TF_INPUTPROCESSORPROFILE active;
    bool has_active = active_profile(active);
    return for_each_profile([&](ITfInputProcessorProfileMgr *, ITfInputProcessorProfiles *profiles, const TF_INPUTPROCESSORPROFILE &p) {
        InputMethod m;
        fill(m, p, profiles, has_active && same_profile(p, active));
        methods.push_back(m);
        return false;
    });
}

int32_t current_input_method(InputMethod &method) {
    ComScope com;
    TF_INPUTPROCESSORPROFILE active;
    if (!active_profile(active)) return not_supported;
    ComPtr<ITfInputProcessorProfiles> profiles;
    CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
    fill(method, active, profiles.Get(), true);
    return ok;
}

int32_t select_input_method(const std::u16string &id) {
    ComScope com;
    int32_t result = not_found;
    int32_t status = for_each_profile([&](ITfInputProcessorProfileMgr *manager, ITfInputProcessorProfiles *, const TF_INPUTPROCESSORPROFILE &p) {
        if (profile_id(p) != id) return false;
        HRESULT hr = manager->ActivateProfile(p.dwProfileType, p.langid, p.clsid, p.guidProfile, p.hkl,
                                              TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
        result = SUCCEEDED(hr) ? ok : failed;
        return true;
    });
    return status == ok ? result : status;
}

// --- reading -----------------------------------------------------------------------------

int32_t set_reading_capture(bool enable) {
    std::lock_guard<std::mutex> lock(reading_mutex);
    if (!enable) {
        remove_hook();
        return ok;
    }
    reading_text.clear();
    reading_candidate.clear();
    if (!reading_hook) reading_hook = SetWindowsHookExW(WH_CALLWNDPROC, call_window_proc, nullptr, GetCurrentThreadId());
    return reading_hook ? ok : failed;
}

int32_t get_reading(std::u16string &reading, bool clear) {
    std::lock_guard<std::mutex> lock(reading_mutex);
    reading = reading_text;
    if (clear) reading_text.clear();
    return ok;
}

int32_t phonetic(const std::u16string &text, std::u16string &reading) {
    reading.clear();
    ComScope com;
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"MSIME.Japan", &clsid))) return not_supported;
    ComPtr<IFELanguage> language;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&language)))) return not_supported;
    if (FAILED(language->Open())) return not_supported;
    BSTR input = SysAllocStringLen((const OLECHAR *)text.data(), UINT(text.size()));
    BSTR output = nullptr;
    HRESULT hr = input ? language->GetPhonetic(input, 1, -1, &output) : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) reading = from_bstr(output);
    SysFreeString(output);
    SysFreeString(input);
    language->Close();
    return SUCCEEDED(hr) ? ok : failed;
}

}
