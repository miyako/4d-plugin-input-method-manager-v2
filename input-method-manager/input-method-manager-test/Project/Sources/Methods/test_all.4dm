//%attributes = {"invisible":true}
If (Application info.headless)
	
	test_constants
	test_convert_kana
	test_phonetic
	test_input_methods
	test_status
	test_reading
	
	// end the line: tool4d can print a warning right after it, and CI needs an exact "PASS" line
	LOG EVENT(Into system standard outputs; "PASS"+Char(Line feed); Information message)
	
End if 
