//%attributes = {"invisible":true}
If (Application info.headless)
	
	test_constants
	test_convert_kana
	test_phonetic
	test_input_methods
	test_status
	test_reading
	
	LOG EVENT(Into system standard outputs; "PASS"; Information message)
	
End if 
