#pragma once
#include <algorithm>
#include <iostream>
#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>

struct offsets_t {
	long block_size;
	char block_type[2];
	short count;
	long first;
	long hash;
};

struct key_block_t {
	long block_size;
	char block_type[2];
	char dummya[18];
	int subkey_count;
	char dummyb[4];
	int subkeys;
	char dummyc[4];
	int value_count;
	int offsets;
	char dummyd[28];
	short len;
	short du;
	char name[255];
};

struct value_block_t {
	long block_size;
	char block_type[2];
	short name_len;
	long size;
	long offset;
	long value_type;
	short flags;
	short dummy;
	char name[255];
};

class hive_key_t {
	key_block_t* key_block;
	uintptr_t main_root;

public:
	explicit hive_key_t( ): key_block( nullptr ), main_root( 0 ) {
	}

	explicit hive_key_t( key_block_t* a, const uintptr_t b ): key_block( a ), main_root( b ) {
	}

	[[nodiscard]] std::vector<std::string> subkeys_list( ) const {
		const auto item = reinterpret_cast<offsets_t*>( this->main_root + key_block->subkeys );
		if ( item->block_type[1] != 'f' && item->block_type[1] != 'h' )
			return { };

		std::vector<std::string> out;
		for ( auto i = 0; i < key_block->subkey_count; i++ ) {
			const auto subkey = reinterpret_cast<key_block_t*>( ( &item->first )[i * 2] + this->main_root );
			if ( !subkey )
				continue;

			out.emplace_back( subkey->name, subkey->len );
		}

		return out;
	}

	[[nodiscard]] std::vector<std::string> keys_list( ) const {
		if ( !key_block->value_count )
			return { };

		std::vector<std::string> out;
		for ( auto i = 0; i < key_block->value_count; i++ ) {
			const auto value = reinterpret_cast<value_block_t*>( reinterpret_cast<int*>( key_block->offsets + this->main_root + 4 )[i] + this->main_root );
			if ( !value )
				continue;

			out.emplace_back( value->name, value->name_len );
		}

		return out;
	}

	template <class T>
	T get_key_value( const std::string& name ) {
		for ( auto i = 0; i < key_block->value_count; i++ ) {
			const auto value = reinterpret_cast<value_block_t*>( reinterpret_cast<int*>( key_block->offsets + this->main_root + 4 )[i] + this->main_root );
			if ( !value || std::string( value->name, value->name_len ) != name )
				continue;

			auto data = reinterpret_cast<char*>( this->main_root + value->offset + 4 );
			if ( value->size & 1 << 31 )
				data = reinterpret_cast<char*>( &value->offset );

			if constexpr ( std::is_same_v<T, std::string> ) {
				if ( value->value_type != REG_SZ && value->value_type != REG_EXPAND_SZ )
					return "";

				std::string text;
				for ( auto j = 0; j < ( value->size & 0xffff ); j++ ) {
					text.append( &data[j] );
				}

				return text;
			} else if constexpr ( std::is_same_v<T, std::vector<std::string>> ) {
				if ( value->value_type != REG_MULTI_SZ )
					return { };

				std::string text;
				std::vector<std::string> out;
				for ( auto j = 0; j < ( value->size & 0xffff ); j++ ) {
					if ( data[j] == '\0' && data[j + 1] == '\0' && data[j + 2] == '\0' ) {
						if ( !text.empty( ) )
							out.emplace_back( text );
						
						text.clear( );
					} else {
						text += data[j];
					}
				}

				return out;
			} else if constexpr ( std::is_same_v<T, int> ) {
				if ( value->value_type != REG_DWORD )
					return 0;

				return *reinterpret_cast<T*>( data );
			}
		}

		return { };
	}
};

class hive_parser {
	struct hive_subpaths_t {
		std::string path;
		hive_key_t data;
	};

	struct hive_cache_t {
		hive_key_t data;
		std::vector<hive_subpaths_t> subpaths;
	};

	key_block_t* main_key_block_data;
	uintptr_t main_root;
	std::vector<char> file_data;
	std::unordered_map<std::string, hive_cache_t> subkey_cache;

	void reclusive_search( const key_block_t* key_block_data, const std::string& current_path, const bool is_reclusive = false ) {
		if ( !key_block_data )
			return;

		const auto item = reinterpret_cast<offsets_t*>( main_root + key_block_data->subkeys );
		if ( item->block_type[1] != 'f' && item->block_type[1] != 'h' )
			return;

		for ( auto i = 0; i < item->count; i++ ) {
			const auto subkey = reinterpret_cast<key_block_t*>( ( &item->first )[i * 2] + main_root );
			if ( !subkey )
				continue;

			std::string subkey_name( subkey->name, subkey->len );
			std::string full_path = current_path.empty( ) ? subkey_name : std::string( current_path ).append( "/" ).append( subkey_name );

			if ( !is_reclusive )
				subkey_cache.try_emplace( full_path, hive_cache_t{ hive_key_t{ subkey, main_root }, std::vector<hive_subpaths_t>{ } } );

			const auto extract_main_key = [ ]( const std::string& str ) -> std::string{
				const size_t slash_pos = str.find( '/' );
				if ( slash_pos == std::string::npos )
					return str;

				return str.substr( 0, slash_pos );
			};

			if ( subkey->subkey_count > 0 ) {
				reclusive_search( subkey, full_path, true );
				subkey_cache.at( extract_main_key( full_path ) ).subpaths.emplace_back( hive_subpaths_t{ full_path, hive_key_t{ subkey, main_root } } );
			} else {
				subkey_cache.at( extract_main_key( full_path ) ).subpaths.emplace_back( full_path, hive_key_t{ subkey, main_root } );
			}
		}
	}

public:
	explicit hive_parser( const std::string& file_directory ) {
		std::ifstream file( file_directory, std::ios::binary );
		if ( !file.is_open( ) )
			return;

		file.seekg( 0, std::ios::end );
		const std::streampos size = file.tellg( );
		if ( size < 0x1020 )
			return;

		file.seekg( 0, std::ios::beg );

		file_data.resize( size );
		file.read( file_data.data( ), size );
		file.close( );

		main_key_block_data = reinterpret_cast<key_block_t*>( reinterpret_cast<uintptr_t>( file_data.data( ) + 0x1020 ) );
		main_root = reinterpret_cast<uintptr_t>( main_key_block_data ) - 0x20;

		reclusive_search( main_key_block_data, "" );
	}

	explicit hive_parser( const std::vector<char>& input_data ) {
		if ( input_data.size( ) < 0x1020 )
			return;

		file_data = input_data;

		main_key_block_data = reinterpret_cast<key_block_t*>( reinterpret_cast<uintptr_t>( file_data.data( ) + 0x1020 ) );
		main_root = reinterpret_cast<uintptr_t>( main_key_block_data ) - 0x20;

		reclusive_search( main_key_block_data, "" );
	}


	[[nodiscard]] bool success( ) const {
		return !subkey_cache.empty( );
	}

	[[nodiscard]] hive_key_t get_subkey( const std::string& key_name, const std::string& path ) const {
		if ( !subkey_cache.contains( key_name ) )
			return hive_key_t{ };

		const auto hive_block = subkey_cache.at( key_name );
		for ( const auto& hive : hive_block.subpaths ) {
			if ( hive.path == path )
				return hive.data;
		}

		return hive_key_t{ };
	}
};
