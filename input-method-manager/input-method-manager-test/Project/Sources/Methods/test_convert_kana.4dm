//%attributes = {"invisible":true,"preemptive":"capable"}
// Half-width katakana with voiced and semi-voiced marks are joined
assert_text(IM Convert kana("ﾊﾟﾋﾟﾌﾟﾍﾟﾎﾟ"; IM kana hiragana); "ぱぴぷぺぽ")
assert_text(IM Convert kana("ｶﾞｷﾞｸﾞｹﾞｺﾞ"; IM kana katakana); "ガギグゲゴ")
assert_text(IM Convert kana("ｳﾞｧｲｵﾘﾝ"; IM kana hiragana); "ゔぁいおりん")

// Hiragana and katakana
assert_text(IM Convert kana("きょうと"; IM kana katakana); "キョウト")
assert_text(IM Convert kana("トウキョウ"; IM kana hiragana); "とうきょう")
assert_text(IM Convert kana("ヷヺ"; IM kana hiragana); "ヷヺ")  // no hiragana form

// Half-width: voiced kana split, full-width ASCII and the ideographic space narrowed
assert_text(IM Convert kana("がっこう　パン"; IM kana halfwidth katakana); "ｶﾞｯｺｳ ﾊﾟﾝ")
assert_text(IM Convert kana("ヴァイオリン"; IM kana halfwidth katakana); "ｳﾞｧｲｵﾘﾝ")
assert_text(IM Convert kana("ＡＢＣ（株）１２３ー"; IM kana halfwidth katakana); "ABC(株)123ｰ")

// Kanji and Latin letters are kept; the full-width forms keep ASCII as it is
assert_text(IM Convert kana("東京ｶﾞｽ ABC"; IM kana katakana); "東京ガス ABC")

// Default form (omitted parameter) is hiragana; an unknown form returns ""
assert_text(IM Convert kana("カタカナ"); "かたかな")
assert_text(IM Convert kana("カタカナ"; 9); "")
assert_text(IM Convert kana(""; IM kana katakana); "")
