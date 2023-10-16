# windows-hive-parser

Example Usage:
```c
int main( ) {
	const auto parser = hive_parser( R"(C:\regdump\SYSTEM)" );
	if ( !parser.success( ) )
		return -1;

	auto bits_subkey = parser.get_subkey( R"(ControlSet001)", R"(ControlSet001/Services/BITS)" );
	if ( bits_subkey.has_value( ) ) {
		std::cout << "Keys: " << std::endl;
		for ( const auto& keys : bits_subkey->keys_list(  ) )
			std::cout << keys << std::endl;

		std::cout << std::endl << "Sub-Keys: " << std::endl;

		for ( const auto& sub_keys : bits_subkey->subkeys_list(  ) )
			std::cout << sub_keys << std::endl;

		std::cout << std::endl;

		if ( const auto delayed_autostart = bits_subkey->get_key_value<int>( "DelayedAutostart" ); delayed_autostart.has_value( ))
			std::cout << "DelayedAutostart: " << delayed_autostart.value( ) << std::endl; //REG_DWORD type

		if ( const auto display_name = bits_subkey->get_key_value<std::string>( "DisplayName" ); display_name.has_value( ))
			std::cout << "DisplayName: " << display_name.value( ) << std::endl; //REG_STR type

		if ( const auto image_path = bits_subkey->get_key_value<std::string>( "ImagePath" ); image_path.has_value( ))
			std::cout << "ImagePath: " << image_path.value( ) << std::endl; //REG_EXPAND_SZ type

		std::cout << "RequiredPrivileges: "; //REG_MULTI_SZ type
		if ( const auto required_privileges = bits_subkey->get_key_value<std::vector<std::string>>( "RequiredPrivileges" ); required_privileges.has_value( ) )
			for ( const auto& val : required_privileges.value( ) )
				std::cout << val << ", ";

		std::cout << std::endl;

		std::cout << "FailureActions: "; //REG_BINARY type
		if ( const auto failure_actions = bits_subkey->get_key_value<std::vector<uint8_t>>( "FailureActions" ); failure_actions.has_value( ) )
			for ( const auto& val : failure_actions.value( ) )
				std::cout << val;

		std::cout << std::endl;
	}

	return 0;
}
```
Output:

![image](https://github.com/reahly/windows-hive-parser/assets/31766694/282f68c0-732a-4d0f-851f-c3b4d8512d6d)

