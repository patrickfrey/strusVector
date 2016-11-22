/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Multimap template including serialization routines using faster unordered map as base
#ifndef _STRUS_INDEX_LIST_MAP_HPP_INCLUDED
#define _STRUS_INDEX_LIST_MAP_HPP_INCLUDED
#include "strus/base/hton.hpp"
#include "utils.hpp"
#include "internationalization.hpp"
#include <vector>
#include <map>


namespace strus {

struct IndexListRef
{
	std::size_t start;
	std::size_t size;

	IndexListRef( std::size_t start_, std::size_t size_)
		:start(start_),size(size_){}
	IndexListRef()
		:start(0),size(0){}
	IndexListRef( const IndexListRef& o)
		:start(o.start),size(o.size){}
};

template <typename KeyType, typename ValueType>
class IndexListMap
{
public:
	IndexListMap()
		:m_listmap(),m_list(),m_maxkey(0){}
	IndexListMap( const IndexListMap& o)
		:m_listmap(o.m_listmap),m_list(o.m_list),m_maxkey(o.m_maxkey){}

	void clear()
	{
		m_listmap.clear();
		m_list.clear();
	}

	void add( const KeyType& key, const std::vector<ValueType>& values)
	{
		if (key > m_maxkey) m_maxkey = key;
		typename ListMap::const_iterator li = m_listmap.find( key);
		if (li != m_listmap.end()) throw strus::runtime_error(_TXT("index list map key added twice"));
		IndexListRef ref( m_list.size(), values.size());
		m_list.insert( m_list.end(), values.begin(), values.end());
		m_listmap[ key] = ref;
	}

	std::vector<ValueType> getValues( const KeyType& key) const
	{
		std::vector<ValueType> rt;
		typename ListMap::const_iterator li = m_listmap.find( key);
		if (li == m_listmap.end()) return rt;
		typename ValueList::const_iterator
			vi = m_list.begin() + li->second.start,
			ve = m_list.begin() + li->second.start + li->second.size;
		rt.insert( rt.end(), vi, ve);
		return rt;
	}

	KeyType maxkey() const
	{
		return m_maxkey;
	}

private:
	typedef std::map<KeyType,IndexListRef> ListMap;
	typedef std::vector<ValueType> ValueList;
	ListMap m_listmap;
	ValueList m_list;
	KeyType m_maxkey;
};


typedef IndexListMap<SampleIndex,ConceptIndex> SampleConceptIndexMap;
typedef IndexListMap<ConceptIndex,SampleIndex> ConceptSampleIndexMap;

}//namespace
#endif

