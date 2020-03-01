/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Normalization of tokens as vector storage database keys
/// \file sentenceLexerConfig.cpp
#include "sentenceLexerConfig.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/localErrorBuffer.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/string_conv.hpp"
#include "strus/constants.hpp"
#include "internationalization.hpp"
#include <cstring>

using namespace strus;

static bool isDigit( char ch)
{
	return (ch >= '0' && ch <= '9');
}

static bool isAlnum( char ch)
{
	return ((ch|32) >= 'a' && (ch|32) <= 'z') || isDigit(ch) || ch == '_' || ch == '-';
}

static std::map<std::string,int> parseTypePriorityMap( const std::string& src)
{
	std::map<std::string,int> rt;
	int priority = 0;
	char const* si = src.c_str();
	while (*si)
	{
		for (; *si && (unsigned char)*si <= 32; ++si){}
		if (*si == ':' || *si == '/') {++si; ++priority; continue;}
		if (isAlnum(*si))
		{
			char const* typenam = si; 
			for (++si; isAlnum(*si); ++si){}
			rt[ std::string( typenam, si-typenam)] = priority;
		}
		if (*si == ',') {++si; continue;}
	}
	return rt;
}

void SentenceLexerConfig::initDefaults()
{
	spaceSubst = defaultSpaceSubst();
	linkSubst = defaultLinkSubst();
	similarityDistance = strus::Constants::defaultGroupSimilarityDistance();
	std::string typepriostr = defaultTypesConfig();
	typepriomap = parseTypePriorityMap( typepriostr);
}

SentenceLexerConfig::SentenceLexerConfig()
{
	initDefaults();
}

SentenceLexerConfig::SentenceLexerConfig( const SentenceLexerConfig& o)
	:spaceSubst(o.spaceSubst)
	,linkSubst(o.linkSubst)
	,similarityDistance(o.similarityDistance)
	,typepriomap(o.typepriomap){}

SentenceLexerConfig::SentenceLexerConfig( const std::string& cfgstr_)
{
	initDefaults();
	load( cfgstr_);
}

void SentenceLexerConfig::load( const std::string& cfgstr_)
{
	strus::LocalErrorBuffer errhnd;
	std::string cfgstr = cfgstr_;

	std::string spaceSubstStr;
	if (strus::extractStringFromConfigString( spaceSubstStr, cfgstr, "spacesb", &errhnd))
	{
		if (spaceSubstStr.size() != 1)
		{
			throw strus::runtime_error(_TXT("invalid empty space substitution char definition (expected single ASCII character): '%s'"), spaceSubstStr.c_str());
		}
		spaceSubst = spaceSubstStr[ 0];
	}
	std::string linkSubstStr;
	if (strus::extractStringFromConfigString( linkSubstStr, cfgstr, "linksb", &errhnd))
	{
		if (linkSubstStr.size() != 1)
		{
			throw strus::runtime_error(_TXT("invalid empty space substitution char definition (expected single ASCII character): '%s'"), linkSubstStr.c_str());
		}
		linkSubst = linkSubstStr[ 0];
	}
	
	if (strus::extractFloatFromConfigString( similarityDistance, cfgstr, "coversim", &errhnd))
	{}

	std::string typePrioStr;
	if (strus::extractStringFromConfigString( typePrioStr, cfgstr, "types", &errhnd))
	{
		typepriomap = parseTypePriorityMap( typePrioStr);
	}
	if (errhnd.hasError())
	{
		throw std::runtime_error( errhnd.fetchError());
	}
}



