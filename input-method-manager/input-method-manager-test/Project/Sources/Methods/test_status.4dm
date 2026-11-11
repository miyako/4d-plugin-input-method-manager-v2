//%attributes = {"invisible":true}
/*
tool4d has no window and no text field with focus, so the commands that act on the
focused window report that; nothing here changes the machine's input source.
*/
var $conversion; $sentence; $result; $mode : Integer
var $composition : Object

$result:=IM Get mode($conversion; $sentence)
ASSERT(($result=0) | ($result=1))
If ($result=1)
	ASSERT($sentence>=0)
End if 

$mode:=IM Get input mode
ASSERT(($mode>=IM mode unknown) & ($mode<=IM mode halfwidth alphanumeric))

$result:=IM Get open status
ASSERT(($result=0) | ($result=1) | ($result=IM error no window) | ($result=IM error no input method))

ASSERT(IM Set input mode(99)=IM error invalid argument)
ASSERT(IM Set input mode(-1)=IM error invalid argument)

$composition:=IM Get composition
ASSERT($composition.active=False)
ASSERT($composition.text="")
ASSERT(Value type($composition.status)=Is real)

If (Not(Is macOS))
	// no focused window on a CI runner
	ASSERT(IM Associate=0)
	ASSERT(IM Disassociate=0)
	ASSERT(IM Complete composition=IM error no window)
	ASSERT(IM Cancel composition=IM error no window)
End if 
