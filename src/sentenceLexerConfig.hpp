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
		return "E,N,V,A,C,W";
	}
	static double defaultSpeedRecallFactor()
	{
		return 0.8;
	}

	void initDefaults();
	void load( const std::string& cfgstr);

	char spaceSubst;				//< character replacing seqences of spaces without a linking character
	char linkSubst;					//< character replacing seqences of linking characters (with or without spaces)
	double groupSimilarityDistance;			//< vector distance minimum defining similarity (value between 0.0 and 1.0)
	double speedRecallFactor;			//< speed recall for similar vector seach
	std::map<std::string,int> typepriomap;		//< type priority map
};

}// namespace
#endif


