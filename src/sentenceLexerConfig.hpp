/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Normalization of tokens as vector storage database keys
/// \file sentenceLexerConfig.hpp
#ifndef _STRUS_SENTENCE_LEXER_CONFIG_HPP_INCLUDED
#define _STRUS_SENTENCE_LEXER_CONFIG_HPP_INCLUDED
#include <vector>
#include <string>
#include <map>

namespace strus {

struct SentenceLexerConfig
{
public:
	SentenceLexerConfig();
	SentenceLexerConfig( const std::string& cfgstr);
	SentenceLexerConfig( const SentenceLexerConfig& o);

	std::vector<std::string> normalizeSource( const std::string& source) const;

	static const char* defaultLinkCharDef()
	{
		return "’`'?!/;:.,–-— )(+&%*#^[]{}<>_";
	}
	static const char* defaultSeparatorCharDef()
	{
		return "\";.:";
	}
	static const char* defaultSpaceCharDef()
	{
		return "\t\b\n\r ";
	}
	static const char defaultSpaceSubst()
	{
		return '_';
	}
	static const char defaultLinkSubst()
	{
		return '-';
	}
	static const char* defaultTypesConfig()
	{
		return "E / N,V";
	}

	void initDefaults();

	struct CharDef
	{
		char uchr[ 4];

		CharDef( int uchr_);
		CharDef( const CharDef& o);
	};

	std::vector<CharDef> linkChars;			//< characters interpreted as entity linking characters
	std::vector<CharDef> spaceChars;		//< space characters besides ASCII <= 32
	std::vector<CharDef> separatorChars;		//< characters interpreted as field separators
	char spaceSubst;				//< character replacing seqences of spaces without a linking character
	char linkSubst;					//< character replacing seqences of linking characters (with or without spaces)
	double similarityDistance;			//< vector distance minimum defining similarity (value between 0.0 and 1.0)
	std::map<std::string,int> typepriomap;		//< type priority map
};

}// namespace
#endif


