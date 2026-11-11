//%attributes = {"invisible":true,"preemptive":"capable"}
// 4D's "=" on Text ignores the difference between hiragana, katakana and half-width
// katakana ("か"="カ" is True), so kana results are compared by character codes.
#DECLARE($actual : Text; $expected : Text)
ASSERT(Compare strings($actual; $expected; sk char codes)=0; "expected \""+$expected+"\", got \""+$actual+"\"")
