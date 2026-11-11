/*
 Portable kana conversion used by the reading and phonetic commands.

 All functions work on UTF-16 text. Characters that have no counterpart in the target form
 (kanji, Latin letters for the full-width kana forms, katakana such as ヰ that has no
 half-width form) are copied unchanged.
*/

#ifndef IMM_KANA_H
#define IMM_KANA_H

#include <string>

namespace kana {

enum Form {
    hiragana = 0,            // IM kana hiragana
    katakana = 1,            // IM kana katakana
    halfwidth_katakana = 2   // IM kana halfwidth katakana
};

bool is_valid_form(int form);

// Half-width katakana (U+FF61..U+FF9F) to full-width katakana, joining a following
// half-width voiced or semi-voiced sound mark with its base.
std::u16string widen_katakana(const std::u16string &s);

std::u16string to_hiragana(const std::u16string &s);  // also widens half-width katakana
std::u16string to_katakana(const std::u16string &s);  // also widens half-width katakana

// Kana to half-width katakana. Full-width ASCII (U+FF01..U+FF5E) and the ideographic space
// are narrowed too, the form expected by Japanese bank transfer (Zengin) data.
std::u16string to_halfwidth_katakana(const std::u16string &s);

std::u16string convert(const std::u16string &s, Form form);

bool has_kanji(const std::u16string &s);              // CJK ideographs or iteration marks
bool is_kana_only(const std::u16string &s);           // hiragana, katakana, prolonged sound mark
bool has_kana_or_kanji(const std::u16string &s);

}

#endif
