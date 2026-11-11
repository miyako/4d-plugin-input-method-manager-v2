//%attributes = {"invisible":true}
var $result : Integer

$result:=IM Set reading capture(1)
If (Is macOS)
	ASSERT($result=IM error not supported)
Else 
	ASSERT($result=IM error none)
End if 
// nothing was typed
ASSERT(IM Get reading=IM Get reading(IM kana katakana))
ASSERT(IM Get reading="")
ASSERT(IM Get reading(9)="")

$result:=IM Set reading capture(0)
ASSERT(($result=IM error none) | ($result=IM error not supported))
