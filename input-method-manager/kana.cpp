#include "kana.h"

namespace kana {

namespace {

// Full-width counterpart of U+FF61..U+FF9F.
const char16_t half_to_full[] = {
    0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, // FF61..FF68
    0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC, // FF69..FF70
    0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, // FF71..FF78
    0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF, // FF79..FF80
    0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, // FF81..FF88
    0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, // FF89..FF90
    0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, // FF91..FF98
    0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x309B, 0x309C        // FF99..FF9F
};

const char16_t HALF_FIRST = 0xFF61;
const char16_t HALF_LAST = 0xFF9F;
const char16_t HALF_VOICED = 0xFF9E;
const char16_t HALF_SEMI_VOICED = 0xFF9F;

// Katakana that take the voiced sound mark: the voiced form is the code point + 1.
bool takes_voiced(char16_t c) {
    if (c >= 0x30AB && c <= 0x30C2) return (c - 0x30AB) % 2 == 0;  // カ..チ
    switch (c) {
        case 0x30C4: case 0x30C6: case 0x30C8:                       // ツ テ ト
        case 0x30CF: case 0x30D2: case 0x30D5: case 0x30D8: case 0x30DB: // ハ..ホ
            return true;
        default:
            return false;
    }
}

bool takes_semi_voiced(char16_t c) {
    switch (c) {
        case 0x30CF: case 0x30D2: case 0x30D5: case 0x30D8: case 0x30DB:
            return true;
        default:
            return false;
    }
}

// Half-width form of a full-width katakana or punctuation, or 0.
char16_t full_to_half(char16_t c) {
    for (char16_t h = HALF_FIRST; h <= HALF_LAST; ++h) {
        if (half_to_full[h - HALF_FIRST] == c) return h;
    }
    return 0;
}

bool is_hiragana(char16_t c) { return (c >= 0x3041 && c <= 0x3096) || c == 0x309D || c == 0x309E; }
bool is_katakana(char16_t c) { return (c >= 0x30A1 && c <= 0x30F6) || c == 0x30FD || c == 0x30FE; }

}

bool is_valid_form(int form) {
    return form == hiragana || form == katakana || form == halfwidth_katakana;
}

std::u16string widen_katakana(const std::u16string &s) {
    std::u16string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char16_t c = s[i];
        if (c < HALF_FIRST || c > HALF_LAST) {
            out += c;
            continue;
        }
        char16_t full = half_to_full[c - HALF_FIRST];
        char16_t next = i + 1 < s.size() ? s[i + 1] : 0;
        if (next == HALF_VOICED) {
            if (takes_voiced(full)) { out += char16_t(full + 1); ++i; continue; }
            if (full == 0x30A6) { out += char16_t(0x30F4); ++i; continue; } // ヴ
            if (full == 0x30EF) { out += char16_t(0x30F7); ++i; continue; } // ヷ
            if (full == 0x30F2) { out += char16_t(0x30FA); ++i; continue; } // ヺ
        } else if (next == HALF_SEMI_VOICED && takes_semi_voiced(full)) {
            out += char16_t(full + 2);
            ++i;
            continue;
        }
        out += full;
    }
    return out;
}

std::u16string to_hiragana(const std::u16string &s) {
    std::u16string out = widen_katakana(s);
    for (char16_t &c : out) {
        if (is_katakana(c)) c = char16_t(c - 0x60);
    }
    return out;
}

std::u16string to_katakana(const std::u16string &s) {
    std::u16string out = widen_katakana(s);
    for (char16_t &c : out) {
        if (is_hiragana(c)) c = char16_t(c + 0x60);
    }
    return out;
}

std::u16string to_halfwidth_katakana(const std::u16string &s) {
    std::u16string full = to_katakana(s);
    std::u16string out;
    out.reserve(full.size() * 2);
    for (char16_t c : full) {
        if (c >= 0xFF01 && c <= 0xFF5E) { out += char16_t(c - 0xFEE0); continue; }
        if (c == 0x3000) { out += u' '; continue; }
        if (c == 0x3099 || c == 0x309B) { out += HALF_VOICED; continue; }
        if (c == 0x309A || c == 0x309C) { out += HALF_SEMI_VOICED; continue; }
        if (char16_t h = full_to_half(c)) { out += h; continue; }
        if (takes_voiced(char16_t(c - 1))) {
            char16_t h = full_to_half(char16_t(c - 1));
            if (h) { out += h; out += HALF_VOICED; continue; }
        }
        if (takes_semi_voiced(char16_t(c - 2))) {
            char16_t h = full_to_half(char16_t(c - 2));
            if (h) { out += h; out += HALF_SEMI_VOICED; continue; }
        }
        switch (c) {
            case 0x30F4: out += u"ｳﾞ"; continue; // ヴ
            case 0x30F7: out += u"ﾜﾞ"; continue; // ヷ
            case 0x30FA: out += u"ｦﾞ"; continue; // ヺ
            case 0x30F5: out += u'ｶ'; continue;       // ヵ
            case 0x30F6: out += u'ｹ'; continue;       // ヶ
            case 0x30EE: out += u'ﾜ'; continue;       // ヮ
            default: out += c; continue;
        }
    }
    return out;
}

std::u16string convert(const std::u16string &s, Form form) {
    switch (form) {
        case katakana: return to_katakana(s);
        case halfwidth_katakana: return to_halfwidth_katakana(s);
        case hiragana:
        default: return to_hiragana(s);
    }
}

bool has_kanji(const std::u16string &s) {
    for (char16_t c : s) {
        if ((c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF) ||
            (c >= 0xF900 && c <= 0xFAFF) || (c >= 0x3005 && c <= 0x3007) ||
            (c >= 0xD840 && c <= 0xD87E)) // high surrogates of CJK extension planes
            return true;
    }
    return false;
}

bool is_kana_only(const std::u16string &s) {
    if (s.empty()) return false;
    for (char16_t c : s) {
        bool kana = (c >= 0x3041 && c <= 0x3096) || (c >= 0x3099 && c <= 0x309E) ||
                    (c >= 0x30A1 && c <= 0x30FA) || (c >= 0x30FC && c <= 0x30FE) ||
                    (c >= 0xFF66 && c <= 0xFF9F);
        if (!kana) return false;
    }
    return true;
}

bool has_kana_or_kanji(const std::u16string &s) {
    if (has_kanji(s)) return true;
    for (char16_t c : s) {
        if ((c >= 0x3041 && c <= 0x30FF) || (c >= 0xFF66 && c <= 0xFF9F)) return true;
    }
    return false;
}

}
