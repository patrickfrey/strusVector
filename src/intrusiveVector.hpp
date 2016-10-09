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
		const_iterator& operator--()			{--m_itr; return *this;}
		const_iterator operator--(int)			{const_iterator rt(m_itr--); return rt;}

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

	class iterator
	{
	public:
		explicit iterator( ScalarType* itr_=0)
			:m_itr(itr_){}
		iterator& operator++()				{++m_itr; return *this;}
		iterator operator++(int)			{iterator rt(m_itr++); return rt;}
		iterator& operator--()				{--m_itr; return *this;}
		iterator operator--(int)			{iterator rt(m_itr--); return rt;}

		ScalarType* operator->()			{return m_itr;}
		ScalarType& operator*()				{return *m_itr;}

		bool operator==( const iterator& o) const	{return m_itr == o.m_itr;}
		bool operator!=( const iterator& o) const	{return m_itr != o.m_itr;}
		bool operator>=( const iterator& o) const	{return m_itr >= o.m_itr;}
		bool operator> ( const iterator& o) const	{return m_itr >  o.m_itr;}
		bool operator<=( const iterator& o) const	{return m_itr <= o.m_itr;}
		bool operator< ( const iterator& o) const	{return m_itr <  o.m_itr;}

	private:
		ScalarType* m_itr;
	};

	const_iterator begin() const		{return const_iterator(m_ar);}
	const_iterator end() const		{return const_iterator(m_ar+m_size);}
	iterator begin()			{return iterator(m_ar);}
	iterator end()				{return iterator(m_ar+m_size);}

	std::size_t size() const		{return m_size;}
	std::size_t origblkidx() const		{return m_origblkidx;}
	const ScalarType* data() const		{return m_ar;}
	ScalarType* data()			{return m_ar;}

	void assignContentCopy( const IntrusiveVector& o)
	{
		if (m_size != o.m_size) throw strus::runtime_error(_TXT("copy of incompatible intrusive vector"));
		std::memcpy( m_ar, o.m_ar, m_size * sizeof(ScalarType));
	}
	void assignContentCopy( const ScalarType* ar_, std::size_t size_)
	{
		if (m_size != size_) throw strus::runtime_error(_TXT("copy of incompatible intrusive vector"));
		std::memcpy( m_ar, ar_, m_size * sizeof(ScalarType));
	}

private:
	std::size_t m_size;
	std::size_t m_origblkidx;
	ScalarType* m_ar;
};

template <typename ScalarType>
class IntrusiveVectorCollection
{
public:
	enum {BlockSize=256};

public:
	explicit IntrusiveVectorCollection( std::size_t vecsize_, std::size_t blksize_=BlockSize)
		:m_blocks(),m_freelist(0),m_vecsize(vecsize_),m_blksize(blksize_),m_blkitr(blksize_)
	{
		if (blksize_ == 0 || vecsize_ * sizeof(ScalarType) < sizeof(FreeListElem))
		{
			throw strus::runtime_error(_TXT("illegal dimension specified for intrusive vector collection"));
		}
	}
	IntrusiveVectorCollection( const IntrusiveVectorCollection& o)
		:m_blocks(),m_freelist(0),m_vecsize(o.m_vecsize),m_blksize(o.m_blksize),m_blkitr(o.m_blksize)
	{
		FreeListElem const* fi = o.m_freelist;
		std::vector<FreeListElem> freear;
		while (fi)
		{
			freear.push_back( *fi);
			fi = fi->next;
		}
		m_blocks.reserve( o.m_blocks.size());
		typename std::vector<ScalarType*>::const_iterator
			bi = o.m_blocks.begin(), be = o.m_blocks.end();
		for (; bi != be; ++bi)
		{
			ScalarType* blk = allocBlock();
			std::memcpy( blk, *bi, m_vecsize * m_blksize * sizeof( ScalarType));
			m_blocks.push_back( blk); //... cannot throw because we reserved enough blocks before
		}
		typename std::vector<FreeListElem>::const_reverse_iterator ei=freear.rbegin(),ee=freear.rend();
		for (; ei != ee; ++ei)
		{
			ScalarType* ptr = m_blocks[ei->blkidx] + ei->elemidx * m_vecsize;
			pushFreeList( ptr, ei->blkidx, ei->elemidx);
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
		std::size_t blkidx;
		ScalarType* ptr = popFreeList( blkidx);
		if (ptr)
		{
			return IntrusiveVector<ScalarType>( m_vecsize, blkidx, ptr);
		}
		else
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
				ptr = m_blocks.back() + m_blkitr * m_vecsize;
				if (*ptr != 0) throw strus::runtime_error( _TXT("detected array bound write"));
				++m_blkitr;
				return IntrusiveVector<ScalarType>( m_vecsize, m_blocks.size()-1, ptr);
			}
		}
	}

	IntrusiveVector<ScalarType> newVectorCopy( const IntrusiveVector<ScalarType>& o)
	{
		IntrusiveVector<ScalarType> rt( newVector());
		rt.assignContentCopy( o);
		return rt;
	}

	void freeVector( IntrusiveVector<ScalarType>& vv)
	{
		std::size_t byteofs = vv.data() - m_blocks[ vv.origblkidx()];
		std::size_t elemidx = byteofs / m_vecsize;
		if (byteofs % m_vecsize != 0 || elemidx >= m_blksize) throw strus::runtime_error( _TXT("illegal free vector call"));
		pushFreeList( vv.data(), vv.origblkidx(), elemidx);
	}

private:
	ScalarType* allocBlock()
	{
		std::size_t mm = InitBlocksSize;
		while (mm && mm <= m_blocks.size()) mm*=2;
		if (!mm) throw std::bad_alloc();
		m_blocks.reserve( mm);
		ScalarType* rt = (ScalarType*)std::calloc( m_vecsize * m_blksize, sizeof( ScalarType));
		if (!rt) throw std::bad_alloc();
		m_blocks.push_back( rt); //... cannot throw because we reserved enough blocks before
		return rt;
	}

private:
	struct FreeListElem
	{
		FreeListElem( std::size_t blkidx_, std::size_t elemidx_)
			:next(0),blkidx(blkidx_),elemidx(elemidx_){}
		FreeListElem( const FreeListElem& o)
			:next(o.next),blkidx(o.blkidx),elemidx(o.elemidx){}

		FreeListElem* next;
		std::size_t blkidx;
		std::size_t elemidx;
	};

	void pushFreeList( ScalarType* ptr, std::size_t blkidx, std::size_t elemidx)
	{
		FreeListElem* fle = (FreeListElem*)ptr;	//... we know what we are doing
		fle->next = m_freelist;
		fle->blkidx = blkidx;
		fle->elemidx = elemidx;
		m_freelist = fle;
	}
	ScalarType* popFreeList( std::size_t& blkidx)
	{
		if (!m_freelist) return 0;
		ScalarType* rt = (ScalarType*)m_freelist;
		blkidx = m_freelist->blkidx;
		m_freelist = m_freelist->next;
		std::memset( rt, 0, m_vecsize * sizeof(ScalarType));
		return rt;
	}

private:
	enum {InitBlocksSize=1024};
	std::vector<ScalarType*> m_blocks;
	FreeListElem* m_freelist;
	std::size_t m_vecsize;
	std::size_t m_blksize;
	std::size_t m_blkitr;
};

}//namespace
#endif

