//%attributes = {"invisible":true}
var $methods : Collection
var $method; $current : Object

$methods:=IM Get input methods
ASSERT($methods#Null)
ASSERT($methods.length>0)
For each ($method; $methods)
	ASSERT(Length($method.id)>0)
	ASSERT(($method.type="keyboard") | ($method.type="ime") | ($method.type="mode"))
	ASSERT(Value type($method.selected)=Is boolean)
	ASSERT(Value type($method.language)=Is text)
End for each 

$current:=IM Get input method
ASSERT(($current.status=IM error none) | ($current.status=IM error not supported))
If ($current.status=IM error none)
	ASSERT(Length($current.id)>0)
	ASSERT($current.selected)
	// the current input method is one of the listed ones
	ASSERT($methods.query("id = :1"; $current.id).length=1)
End if 

// Selecting is not tested with a real identifier: it would change the machine's input source
ASSERT(IM Set input method("no-such-input-method")=IM error not found)
ASSERT(IM Set input method("")=IM error invalid argument)
