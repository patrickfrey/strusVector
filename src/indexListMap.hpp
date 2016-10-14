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
		:m_listmap(),m_list(){}
	IndexListMap( const IndexListMap& o)
		:m_listmap(o.m_listmap),m_list(o.m_list){}

	void clear()
	{
		m_listmap.clear();
		m_list.clear();
	}

	void add( const KeyType& key, const std::vector<ValueType>& values)
	{
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

	std::string serialization() const
	{
		std::string rt;
		unsigned int mapsize_n = ByteOrder<unsigned int>::hton( m_listmap.size());
		rt.append( (const char*)&mapsize_n, sizeof(mapsize_n));

		typename ListMap::const_iterator li = m_listmap.begin(), le = m_listmap.end();
		for (; li != le; ++li)
		{
			KeyType key_n = ByteOrder<KeyType>::hton( li->first);
			uint64_t start_n = ByteOrder<uint64_t>::hton( li->second.start);
			uint32_t size_n = ByteOrder<uint32_t>::hton( li->second.size);

			rt.append( (const char*)&key_n, sizeof(key_n));
			rt.append( (const char*)&start_n, sizeof(start_n));
			rt.append( (const char*)&size_n, sizeof(size_n));
		}
		typename ValueList::const_iterator vi = m_list.begin(), ve = m_list.end();
		for (; vi != ve; ++vi)
		{
			ValueType val_n = ByteOrder<ValueType>::hton( *vi);
			rt.append( (const char*)&val_n, sizeof(val_n));
		}
		return rt;
	}

	static IndexListMap fromSerialization( const std::string& content)
	{
		IndexListMap rt;
		char const* si = content.c_str();
		const char* se = content.c_str() + content.size();
		unsigned int mapsize_n;
		std::memcpy( &mapsize_n, si, sizeof( mapsize_n));
		si += sizeof(mapsize_n);
		unsigned int mapsize = ByteOrder<unsigned int>::ntoh( mapsize_n);

		for (unsigned int mi = 0; si < se && mi < mapsize; ++mi)
		{
			KeyType key = parseElem<KeyType>( si, se);
			uint64_t start = parseElem<uint64_t>( si, se);
			uint32_t size = parseElem<uint32_t>( si, se);
			rt.m_listmap[ key] = IndexListRef( start, size);
		}
		while (si < se)
		{
			ValueType value = parseElem<ValueType>( si, se);
			rt.m_list.push_back( value);
		}
		return rt;
	}

private:
	template <typename ElemType>
	static ElemType parseElem( char const*& si, const char* se)
	{
		ElemType elem_n;
		if (si + sizeof(ElemType) > se) throw strus::runtime_error(_TXT("array bound read in parse index list map"));
		std::memcpy( &elem_n, si, sizeof(elem_n));
		si += sizeof(ElemType);
		return ByteOrder<ElemType>::ntoh( elem_n);
	}

private:
	typedef utils::UnorderedMap<KeyType,IndexListRef> ListMap;
	typedef std::vector<ValueType> ValueList;
	ListMap m_listmap;
	ValueList m_list;
};


}//namespace
#endif

