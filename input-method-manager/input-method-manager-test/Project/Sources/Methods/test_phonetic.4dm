//%attributes = {"invisible":true,"preemptive":"capable"}
var $reading : Text

If (Is macOS)
	assert_text(IM Get phonetic("東京都"); "とうきょうと")
	assert_text(IM Get phonetic("山田太郎"; IM kana katakana); "ヤマダタロウ")
	assert_text(IM Get phonetic("山田太郎"; IM kana halfwidth katakana); "ﾔﾏﾀﾞﾀﾛｳ")
	// Latin letters, digits and kana are kept as typed
	assert_text(IM Get phonetic("ABC商事"); "ABCしょうじ")
	assert_text(IM Get phonetic("第3営業部"); "だい3えいぎょうぶ")
Else 
	// MSIME.Japan is only registered where the Japanese IME is installed
	$reading:=IM Get phonetic("東京都")
	ASSERT((Length($reading)=0) | (Compare strings($reading; "とうきょうと"; sk char codes)=0))
End if 

assert_text(IM Get phonetic(""); "")
assert_text(IM Get phonetic("東京"; 9); "")
