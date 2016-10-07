/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _STRUS_VECTOR_INTRUSIVE_VECTOR_HPP_INCLUDED
#define _STRUS_VECTOR_INTRUSIVE_VECTOR_HPP_INCLUDED
#include "internationalization.hpp"
#include "strus/base/crc32.hpp"
#include <list>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

namespace strus
{

template <typename ScalarType>
class IntrusiveVector
{
public:
	IntrusiveVector()
		:m_size(0),m_origblkidx(0),m_ar(0){}
	IntrusiveVector( std::size_t size_, std::size_t origblkidx_, ScalarType* ar_)
		:m_size(size_),m_origblkidx(origblkidx_),m_ar(ar_){}
	IntrusiveVector( const IntrusiveVector& o)
		:m_size(o.m_size),m_origblkidx(o.m_origblkidx),m_ar(o.m_ar){}

	ScalarType& operator[]( std::size_t idx)		{return m_ar[idx];}
	const ScalarType& operator[]( std::size_t idx) const	{return m_ar[idx];}

	class const_iterator
	{
	public:
		explicit const_iterator( ScalarType const* itr_=0)
			:m_itr(itr_){}
		const_iterator& operator++()			{++m_itr; return *this;}
		const_iterator operator++(int)			{const_iterator rt(m_itr++); return rt;}

		const ScalarType* operator->() const		{return m_itr;}
		const ScalarType& operator*() const		{return *m_itr;}

		bool operator==( const const_iterator& o) const	{return m_itr == o.m_itr;}
		bool operator!=( const const_iterator& o) const	{return m_itr != o.m_itr;}
		bool operator>=( const const_iterator& o) const	{return m_itr >= o.m_itr;}
		bool operator> ( const const_iterator& o) const	{return m_itr >  o.m_itr;}
		bool operator<=( const const_iterator& o) const	{return m_itr <= o.m_itr;}
		bool operator< ( const const_iterator& o) const	{return m_itr <  o.m_itr;}
	private:
		ScalarType const* m_itr;
	};

	std::size_t size() const		{return m_size;}
	std::size_t origblkidx() const		{return m_origblkidx;}
	const ScalarType* data() const		{return m_ar;}

private:
	std::size_t m_size;
	std::size_t m_origblkidx;
	ScalarType* m_ar;
};

template <typename ScalarType>
class IntrusiveVectorCollection
{
public:

public:
	explicit IntrusiveVectorCollection( std::size_t vecsize_, std::size_t blksize_)
		:m_freelist(),m_vecsize(vecsize_),m_blksize(blksize_),m_blkitr(blksize_)
	{
		if (blksize_ == 0 || vecsize_ == 0) throw strus::runtime_error("illegal dimension specified for intrusive vector collection");
	}
	IntrusiveVectorCollection( const IntrusiveVectorCollection& o)
		:m_freelist(o.m_freelist),m_vecsize(o.m_vecsize),m_blksize(o.m_blksize),m_blkitr(o.m_blksize)
	{
		m_blocks.reserve( o.m_blocks.size());
		typename std::vector<ScalarType*>::const_iterator
			bi = o.m_blocks.begin(), be = o.m_blocks.end();
		for (; bi != be; ++bi)
		{
			ScalarType* blk = allocBlock();
			std::memcpy( blk, *bi, m_vecsize * m_blksize, sizeof( ScalarType));
			m_blocks.push_back( blk); //... cannot throw because we reserved enough blocks before
		}
	}

	~IntrusiveVectorCollection()
	{
		typename std::vector<ScalarType*>::iterator
			bi = m_blocks.begin(), be = m_blocks.end();
		for (; bi != be; ++bi) std::free( *bi);
	}

	std::size_t vecsize() const
	{
		return m_vecsize;
	}
	IntrusiveVector<ScalarType> newVector()
	{
		if (m_freelist.empty())
		{
			if (m_blkitr == m_blksize)
			{
				ScalarType* blk = allocBlock();
				m_blkitr = 1;
				return IntrusiveVector<ScalarType>( m_vecsize, m_blocks.size()-1, blk);
				//... first element
			}
			else
			{
				ScalarType* vec = m_blocks.back() + m_blkitr * m_vecsize;
				++m_blkitr;
				return IntrusiveVector<ScalarType>( m_vecsize, m_blocks.size()-1, vec);
			}
		}
		else
		{
			std::size_t vecidx = m_freelist.back();
			m_freelist.pop_back();
			std::size_t blkidx = vecidx / m_blksize;
			std::size_t elemidx = vecidx % m_blksize;
			ScalarType* vec = m_blocks[ blkidx] + elemidx * m_vecsize;
			std::memset( vec, 0, m_vecsize * sizeof(vec));
			return IntrusiveVector<ScalarType>( m_vecsize, blkidx, vec);
		}
	}
	void freeVector( const IntrusiveVector<ScalarType>& vv)
	{
		std::size_t wordofs = (vv.data() - m_blocks[ vv.origblkidx()]) / m_vecsize;
		if (wordofs >= m_blksize) throw strus::runtime_error("illegal free vector call");
		m_freelist.push_back( m_blksize * vv.origblkidx() + wordofs);
	}

private:
	ScalarType* allocBlock()
	{
		m_blocks.reserve( m_blocks.size() +1);
		ScalarType* rt = std::calloc( m_vecsize * m_blksize, sizeof( ScalarType));
		if (!rt)
		{
			throw std::bad_alloc();
		}
		m_blocks.push_back( rt); //... cannot throw because we reserved enough blocks before
		return rt;
	}

private:
	std::vector<ScalarType*> m_blocks;
	std::vector<std::size_t> m_freelist;
	std::size_t m_vecsize;
	std::size_t m_blksize;
	std::size_t m_blkitr;
};

}//namespace
#endif

