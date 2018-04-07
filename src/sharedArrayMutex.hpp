/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for storing similarity group representants (individuals in the genetic algorithm for breeding similarity group representants)
#ifndef _STRUS_VECTOR_SHARED_ARRAY_HPP_INCLUDED
#define _STRUS_VECTOR_SHARED_ARRAY_HPP_INCLUDED
#include "strus/base/malloc.hpp"
#include "strus/base/thread.hpp"
#include "strus/base/platform.hpp"
#include <new>
#include <stdexcept>

namespace strus {

class SharedArrayMutex
{
public:
	SharedArrayMutex( std::size_t arsize_, unsigned int startidx_)
		:m_mutexarmem(0),m_mutexar(0),m_mutexarsize(0),m_arsize(arsize_),m_mod(128),m_startidx(startidx_)
	{
		while (m_arsize > (std::size_t)m_mod * MaxNofMutex)
		{
			m_mod *= 2;
			if (m_mod < 128) throw std::bad_alloc();
		}
		m_arsize = (std::size_t)m_mod * MaxNofMutex;
		m_mutexarsize = (m_arsize) / m_mod;
		m_mutexarmem = strus::aligned_malloc( m_mutexarsize * sizeof(strus::recursive_mutex), strus::platform::CacheLineSize);
		m_mutexar = new (m_mutexarmem)strus::recursive_mutex[ m_mutexarsize];
	}
	~SharedArrayMutex()
	{
		std::size_t mi = 0, me = m_mutexarsize;
		for (; mi != me; ++mi)
		{
			m_mutexar[mi].~Mutex();
		}
		strus::aligned_free( m_mutexarmem);
	}

	unsigned int nofElementsPerMutex()
	{
		return m_mod;
	}

	class Lock
		:private strus::recursive_unique_lock
	{
	public:
		Lock( const SharedArrayMutex* arraymut_, unsigned int idx_)
			:strus::recursive_unique_lock( arraymut_->mutex(idx_))
			,m_arraymut(arraymut_),m_idx(idx_){}
		~Lock(){}

		void skip( unsigned int idx_)
		{
			if ((idx_-m_arraymut->m_startidx) / m_arraymut->m_mod != (m_idx-m_arraymut->m_startidx) / m_arraymut->m_mod)
			{
				strus::recursive_unique_lock::unlock();
				strus::recursive_unique_lock::operator=( strus::recursive_unique_lock( m_arraymut->mutex(idx_)));
				m_idx = idx_;
				strus::recursive_unique_lock::lock();
			}
		}

		unsigned int id() const
		{
			return m_idx;
		}

	private:
		const SharedArrayMutex* m_arraymut;
		unsigned int m_idx;
	};

private:
	friend class Lock;
	strus::recursive_mutex& mutex( unsigned int idx)	const
	{
		return m_mutexar[ (idx-m_startidx) / m_mod];
	}

private:
	enum {MaxNofMutex=1024};
	void* m_mutexarmem;				//< aligned memory for mutex array
	typedef strus::recursive_mutex Mutex;
	mutable Mutex* m_mutexar;			//< mutex array for mutual exclusive read/write access to array elements
	std::size_t m_mutexarsize;			//< number of elements in m_mutexar
	std::size_t m_arsize;				//< (maximum) size of array
	unsigned int m_mod;				//< number of elements per mutex
	unsigned int m_startidx;			//< first index of array (usually 1 or 0)
};

} //namespace
#endif

