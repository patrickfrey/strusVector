/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Memory usage efficient list of strings
#ifndef _STRUS_STRING_LIST_MAP_HPP_INCLUDED
#define _STRUS_STRING_LIST_MAP_HPP_INCLUDED
#include <vector>
#include <string>
#include <cstring>
#include "internationalization.hpp"

namespace strus {

class StringList
{
public:
	StringList()
		:m_indexlist(),m_strings(){}
	StringList( const StringList& o)
		:m_indexlist(o.m_indexlist),m_strings(o.m_strings){}

	void clear()
	{
		m_indexlist.clear();
		m_strings.clear();
	}

	void push_back( const std::string& value)
	{
		if (0!=std::memchr( value.c_str(), '\0', value.size()))
		{
			throw strus::runtime_error( "%s", _TXT("string list element contains null bytes"));
		}
		m_indexlist.push_back( m_strings.size());
		m_strings.append( value);
		m_strings.push_back( '\0');
	}

	const char* operator[]( std::size_t idx) const
	{
		if (idx >= m_indexlist.size()) throw strus::runtime_error( "%s", _TXT("array bound read in string list"));
		return m_strings.c_str() + m_indexlist[idx];
	}

	std::size_t size() const
	{
		return m_indexlist.size();
	}

	std::string serialization() const
	{
		return m_strings;
	}

	static StringList fromSerialization( const std::string& content)
	{
		StringList rt;
		char const* si = content.c_str();
		const char* se = si + content.size();
		while (si < se)
		{
			rt.m_indexlist.push_back( si-content.c_str());
			si = std::strchr( si, '\0');
			++si;
		}
		rt.m_strings.append( content.c_str(), content.size());
		return rt;
	}

private:
	typedef std::vector<std::size_t> IndexList;
	IndexList m_indexlist;
	std::string m_strings;
};


}//namespace
#endif

