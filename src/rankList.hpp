/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Template for temporary ranklist
#ifndef _STRUS_VECTOR_RANKLIST_TEMPLATE_HPP_INCLUDED
#define _STRUS_VECTOR_RANKLIST_TEMPLATE_HPP_INCLUDED
#include "strus/index.hpp"
#include "simHash.hpp"
#include "simHashQueryResult.hpp"
#include <utility>
#include <cstring>
#include <ostream>
#include <sstream>
#include <limits>

namespace strus {

template <typename Element>
class RankList
{
public:
	enum {MaxSize=256};

public:
	explicit RankList( int maxNofRanks_)
		:m_nofRanks(0),m_maxNofRanks(maxNofRanks_)
	{
		if (maxNofRanks_ <= 0) throw strus::runtime_error( "%s",  _TXT( "illegal value for maximum number of ranks"));
		if (m_maxNofRanks > MaxSize) throw strus::runtime_error( "%s",  _TXT( "maximum number of ranks is out of range"));
		for (int ii=0; ii<m_maxNofRanks; ++ii) m_brute_index[ii] = (unsigned char)(unsigned int)ii;
	}

	void bruteInsert_at( int idx, const Element& elem)
	{
		int elemidx = m_brute_index[ m_maxNofRanks-1];
		std::memmove( m_brute_index+idx+1, m_brute_index+idx, m_maxNofRanks-idx-1);
		m_brute_index[ idx] = elemidx;
		m_brute_ar[ elemidx] = elem;
	}

	std::size_t size() const
	{
		return m_nofRanks;
	}

	bool empty() const
	{
		return m_nofRanks == 0;
	}

	const Element& back() const
	{
		if (m_nofRanks == 0) throw std::runtime_error(_TXT("array bound read in ranklist"));
		int endidx = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		return m_brute_ar[ m_brute_index[ endidx-1]];
	}

	bool complete() const
	{
		return m_nofRanks > m_maxNofRanks;
	}

	bool insert( const Element& elem)
	{
		if (m_nofRanks == 0)
		{
			m_brute_ar[ m_brute_index[ 0] = 0] = elem;
		}
		else if (!(elem < back()))
		{
			if (m_nofRanks < m_maxNofRanks)
			{
				bruteInsert_at( m_nofRanks, elem);
			}
			else
			{
				return false;
			}
		}
		else
		{
			int first = 0;
			int last = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
			int mid = last-1;

			while (last - first > 2)
			{
				if (elem < m_brute_ar[ m_brute_index[ mid]])
				{
					last = mid;
					mid = (first + mid) >> 1;
				}
				else if (m_brute_ar[ m_brute_index[ mid]] < elem)
				{
					first = mid+1;
					mid = (last + mid) >> 1;
				}
				else
				{
					bruteInsert_at( mid, elem);
					++m_nofRanks;
					return true;
				}
			}
			for (; first < last; ++first)
			{
				if (elem < m_brute_ar[ m_brute_index[ first]])
				{
					bruteInsert_at( first, elem);
					break;
				}
			}
			if (first == last && last < m_maxNofRanks)
			{
				bruteInsert_at( last, elem);
			}
		}
		++m_nofRanks;
		return true;
	}

	class const_iterator
	{
	public:
		typedef Element value_type;

		const_iterator( const unsigned char* bstart, const Element* data_)
			:m_ai(bstart),m_data(data_){}
		const_iterator( const const_iterator& o)
			:m_ai(o.m_ai),m_data(o.m_data){}
		const_iterator& operator++()			{++m_ai; return *this;}
		const_iterator& operator++(int)			{const_iterator rt(*this); ++m_ai; return rt;}

		const Element& operator*() const		{return m_data[ *m_ai];}
		const Element* operator->() const		{return m_data + *m_ai;}

		bool operator<( const const_iterator& o) const	{return m_ai < o.m_ai;}
		bool operator<=( const const_iterator& o) const	{return m_ai <= o.m_ai;}
		bool operator>( const const_iterator& o) const	{return m_ai > o.m_ai;}
		bool operator>=( const const_iterator& o) const	{return m_ai >= o.m_ai;}
		bool operator==( const const_iterator& o) const	{return m_ai == o.m_ai;}
		bool operator!=( const const_iterator& o) const	{return m_ai != o.m_ai;}

	private:
		unsigned char const* m_ai;
		const Element* m_data;
	};

	const_iterator begin() const
	{
		return const_iterator( m_brute_index, m_brute_ar);
	}
	const_iterator end() const
	{
		return const_iterator( m_brute_index + (m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks), m_brute_ar);
	}

private:
	int m_nofRanks;
	int m_maxNofRanks;
	unsigned char m_brute_index[ MaxSize];
	Element m_brute_ar[ MaxSize];
};

}//namespace
#endif

