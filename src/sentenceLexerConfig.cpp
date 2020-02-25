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
#include "strus/constants.hpp"
#include "internationalization.hpp"
#include <cstring>

using namespace strus;

typedef SentenceLexerConfig::CharDef CharDef;

SentenceLexerConfig::CharDef::CharDef( int uchr_)
{
	char buf[ 8];
	std::size_t sl = strus::utf8encode( buf, uchr_);
	if (sl > sizeof(uchr)) throw std::runtime_error(_TXT("unicode character definition for sentence lexer out of range"));
	std::memset( uchr, 0, sizeof(uchr));
	std::memcpy( uchr, buf, sl);
}

SentenceLexerConfig::CharDef::CharDef( const CharDef& o)
{
	std::memcpy( uchr, o.uchr, sizeof(uchr));
}

static bool isDigit( char ch)
{
	return (ch >= '0' && ch <= '9');
}

static bool isAlnum( char ch)
{
	return ((ch|32) >= 'a' && (ch|32) <= 'z') || isDigit(ch) || ch == '_' || ch == '-';
}

static int parseNumericEntity( char const*& src)
{
	char const* si = src;
	if (*si == '&')
	{
		++si;
		if (*si == '#')
		{
			++si;
			if (*si >= '0' && *si <= '9')
			{
				char const* start = si;
				for (++si; isDigit(*si); ++si){}
				if (*si == ';')
				{
					++si;
					src = si;
					return atoi( start);
				}
			}
		}
	}
	return -1;
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

static std::vector<SentenceLexerConfig::CharDef> parseCharList( const std::string& str)
{
	std::vector<SentenceLexerConfig::CharDef> rt;
	char const* si = str.c_str();
	while (*si)
	{
		if (*si == '&')
		{
			int ne = parseNumericEntity( si);
			if (ne >= 0)
			{
				rt.push_back( SentenceLexerConfig::CharDef( ne));
			}
			else
			{
				rt.push_back( SentenceLexerConfig::CharDef( *si));
				++si;
			}
		}
		else
		{
			unsigned char sl = strus::utf8charlen( *si);
			rt.push_back( SentenceLexerConfig::CharDef( strus::utf8decode( si, sl)));
			si += sl;
		}
	}
	return rt;
}

void SentenceLexerConfig::initDefaults()
{
	spaceSubst = defaultSpaceSubst();
	linkSubst = defaultLinkSubst();
	similarityDistance = strus::Constants::defaultGroupSimilarityDistance();
	linkChars = parseCharList(defaultLinkCharDef());
	spaceChars = parseCharList(defaultSpaceCharDef());
	separatorChars = parseCharList(defaultSeparatorCharDef());
	std::string typepriostr = defaultTypesConfig();
	typepriomap = parseTypePriorityMap( typepriostr);
}

SentenceLexerConfig::SentenceLexerConfig()
{
	initDefaults();
}

SentenceLexerConfig::SentenceLexerConfig( const SentenceLexerConfig& o)
	:linkChars(o.linkChars)
	,spaceChars(o.spaceChars)
	,separatorChars(o.separatorChars)
	,spaceSubst(o.spaceSubst)
	,linkSubst(o.linkSubst)
	,similarityDistance(o.similarityDistance)
	,typepriomap(o.typepriomap){}

SentenceLexerConfig::SentenceLexerConfig( const std::string& cfgstr_)
{
	initDefaults();

	strus::LocalErrorBuffer errhnd;
	std::string cfgstr = cfgstr_;
	std::string linkchrs;
	if (strus::extractStringFromConfigString( linkchrs, cfgstr, "link", &errhnd))
	{
		linkChars = parseCharList( linkchrs);
	}
	std::string spacechrs;
	if (strus::extractStringFromConfigString( spacechrs, cfgstr, "space", &errhnd))
	{
		spaceChars = parseCharList( spacechrs);
	}
	std::string separatorchrs;
	if (strus::extractStringFromConfigString( separatorchrs, cfgstr, "sep", &errhnd))
	{
		separatorChars = parseCharList( separatorchrs);
	}
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

static bool memberOfCharDef( const std::vector<CharDef>& ar, const char* si, int slen)
{
	std::vector<CharDef>::const_iterator ai = ar.begin(), ae = ar.end();
	for (; ai != ae; ++ai)
	{
		if (*si == ai->uchr[0] && 0==std::memcmp( si, ai->uchr, slen)) return true;
	}
	return false;
}

std::vector<std::string> SentenceLexerConfig::normalizeSource( const std::string& source) const
{
	std::vector<std::string> rt;
	std::string current;
	char const* si = source.c_str();
	const char* se = si + source.size();
	while (si < se)
	{
		int cl = strus::utf8charlen( *si);
		if (memberOfCharDef( separatorChars, si, cl))
		{
			if (!current.empty())
			{
				rt.push_back( current);
				current.clear();
			}
		}
		else if (memberOfCharDef( linkChars, si, cl))
		{
			while (!current.empty() && current[ current.size()-1] == spaceSubst)
			{
				current.resize( current.size()-1);
			}
			current.push_back( linkSubst);
		}
		else if ((cl == 1 && (unsigned char)si[0] <= 32) || memberOfCharDef( spaceChars, si, cl))
		{
			while (!current.empty() && current[ current.size()-1] == spaceSubst)
			{
				current.resize( current.size()-1);
			}
			if (!current.empty() && current[ current.size()-1] != linkSubst)
			{
				current.push_back( spaceSubst);
			}
		}
		else
		{
			current.append( si, cl);
		}
		si += cl;
	}
	if (!current.empty())
	{
		rt.push_back( current);
		current.clear();
	}
	return rt;
}




