/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Similarity hash structure
#include "simHash.hpp"
#include "strus/base/bitOperations.hpp"
#include "internationalization.hpp"
#include "strus/base/hton.hpp"
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <limits>

using namespace strus;

static std::size_t SimHash_mallocSize( unsigned int size_)
{
	return SimHash::arsize(size_)*sizeof(uint64_t);
}

SimHash::~SimHash()
{
	if (m_ar) std::free( m_ar);
}

SimHash::SimHash( const SimHash& o)
	:m_ar((uint64_t*)std::malloc( SimHash_mallocSize( o.m_size)))
	,m_size(o.m_size)
{
	if (!m_ar) throw std::bad_alloc();
	std::memcpy( m_ar, o.m_ar, SimHash_mallocSize( m_size));
}

SimHash::SimHash( std::size_t size_, bool initval)
	:m_ar(0),m_size(size_)
{
	if (!initval)
	{
		m_ar = (uint64_t*)std::calloc( arsize( size_), sizeof(uint64_t));
		if (!m_ar) throw std::bad_alloc();
	}
	else
	{
		m_ar = (uint64_t*)std::malloc( SimHash_mallocSize(size_));
		if (!m_ar) throw std::bad_alloc();
		if (m_size == 0) return;

		uint64_t elem = std::numeric_limits<uint64_t>::max();
		std::size_t ii = 0, nn = m_size / NofElementBits;
		for (; ii < nn; ++ii)
		{
			m_ar[ ii] = elem;
		}
		if (m_size > nn * NofElementBits)
		{
			elem <<= m_size - (nn * NofElementBits);
			m_ar[ nn] = elem;
		}
	}
}

SimHash::SimHash( const std::vector<bool>& bv)
	:m_ar((uint64_t*)std::malloc( SimHash_mallocSize( bv.size())))
	,m_size(bv.size())
{
	if (bv.empty()) return;
	if (!m_ar) throw std::bad_alloc();
	uint64_t elem = 0;
	std::vector<bool>::const_iterator bi = bv.begin(), be = bv.end();
	unsigned int bidx = 0;
	unsigned int eidx = 0;
	for (; bi != be; ++bi,++bidx)
	{
		if (bidx == (int)NofElementBits)
		{
			m_ar[ eidx++] = elem;
			elem = 0;
			bidx = 0;
		}
		else
		{
			elem <<= 1;
		}
		elem |= *bi ? 1:0;
	}
	elem <<= (NofElementBits - bidx);
	m_ar[ eidx] = elem;
}

SimHash& SimHash::operator=( const SimHash& o)
{
	if (m_size == 0)
	{
		m_ar = (uint64_t*)std::malloc( SimHash_mallocSize(o.m_size));
		m_size = o.m_size;
	}
	else if (size() != o.size())
	{
		throw strus::runtime_error(_TXT("assignment of incompatible sim hash"));
	}
	std::memcpy( m_ar, o.m_ar, SimHash_mallocSize(m_size));
	return *this;
}

int SimHash::compare( const SimHash& o) const
{
	if (m_size != o.m_size) return (m_size > o.m_size)?1:-1;
	return std::memcmp( m_ar, o.m_ar, SimHash_mallocSize(m_size));
}

bool SimHash::operator[]( std::size_t idx) const
{
	if (idx >= (std::size_t)m_size)
	{
		throw strus::runtime_error(_TXT("array bound read in %s"), "SimHash");
	}
	std::size_t aridx = idx / NofElementBits;
	std::size_t arofs = idx % NofElementBits;
	uint64_t mask = 1;
	mask <<= (NofElementBits-1 - arofs);
	return (m_ar[ aridx] & mask) != 0;
}

void SimHash::set( std::size_t idx, bool value)
{
	if (idx >= (std::size_t)m_size)
	{
		throw strus::runtime_error(_TXT("array bound write in %s"), "SimHash");
	}
	std::size_t aridx = idx / NofElementBits;
	std::size_t arofs = idx % NofElementBits;
	uint64_t mask = 1;
	mask <<= (NofElementBits-1 - arofs);
	if (value)
	{
		m_ar[ aridx] |= mask;
	}
	else
	{
		m_ar[ aridx] &= ~mask;
	}
}

std::vector<std::size_t> SimHash::indices( bool what) const
{
	std::vector<std::size_t> rt;
	uint64_t const* ai = m_ar;
	const uint64_t* ae = m_ar + arsize();
	std::size_t aridx = 0;
	for (; ai != ae; ++ai,++aridx)
	{
		std::size_t arofs = 0;
		uint64_t mask = (uint64_t)1 << (NofElementBits-1);
		for (; arofs < NofElementBits; ++arofs,mask>>=1)
		{
			if (((mask & *ai) != 0) == what)
			{
				rt.push_back( aridx * NofElementBits + arofs);
			}
		}
	}
	return rt;
}

unsigned int SimHash::count() const
{
	unsigned int rt = 0;
	uint64_t const* ai = m_ar;
	const uint64_t* ae = m_ar + arsize();
	for (; ai != ae; ++ai)
	{
		rt += strus::BitOperations::bitCount( *ai);
	}
	return rt;
}

unsigned int SimHash::dist( const SimHash& o) const
{
	unsigned int rt = 0;
	uint64_t const* ai = m_ar;
	const uint64_t* ae = m_ar + arsize();
	uint64_t const* oi = o.m_ar;
	const uint64_t* oe = o.m_ar + arsize();
	for (; oi != oe && ai != ae; ++oi,++ai)
	{
		rt += strus::BitOperations::bitCount( *ai ^ *oi);
	}
	for (; oi != oe; ++oi)
	{
		rt += strus::BitOperations::bitCount( *oi);
	}
	for (; ai != ae; ++ai)
	{
		rt += strus::BitOperations::bitCount( *ai);
	}
	return rt;
}

bool SimHash::near( const SimHash& o, unsigned int dist) const
{
	unsigned int cnt = 0;
	uint64_t const* ai = m_ar;
	const uint64_t* ae = m_ar + arsize();
	uint64_t const* oi = o.m_ar;
	const uint64_t* oe = o.m_ar + arsize();
	for (; oi != oe && ai != ae; ++oi,++ai)
	{
		cnt += strus::BitOperations::bitCount( *ai ^ *oi);
		if (cnt > dist) return false;
	}
	for (; oi != oe; ++oi)
	{
		cnt += strus::BitOperations::bitCount( *oi);
		if (cnt > dist) return false;
	}
	for (; ai != ae; ++ai)
	{
		cnt += strus::BitOperations::bitCount( *ai);
		if (cnt > dist) return false;
	}
	return true;
}

std::string SimHash::tostring() const
{
	std::ostringstream rt;
	uint64_t const* ai = m_ar;
	const uint64_t* ae = m_ar + arsize();
	for (unsigned int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (aidx) rt << '|';
		uint64_t mi = 1;
		unsigned int cnt = m_size - (aidx * NofElementBits);
		mi <<= (NofElementBits-1);
		for (; mi && cnt; mi >>= 1, --cnt)
		{
			rt << ((*ai & mi)?'1':'0');
		}
	}
	return rt.str();
}

std::string SimHash::serialization() const
{
	std::string rt;
	uint32_t size_bits = ByteOrder<uint32_t>::hton( m_size);
	rt.append( (const char*)&size_bits, sizeof(size_bits));
	uint64_t const* ai = m_ar;
	const uint64_t* ae = m_ar + arsize();
	for (; ai != ae; ++ai)
	{
		uint64_t val = ByteOrder<uint64_t>::hton( *ai);
		rt.append( (const char*)&val, sizeof(val));
	}
	return rt;
}

SimHash SimHash::createFromSerialization( const std::string& in)
{
	uint32_t const* nw = (const uint32_t*)(void*)(in.c_str());
	uint32_t size_bits = ByteOrder<uint32_t>::ntoh( *nw++);
	SimHash rt( size_bits, false);
	std::size_t ai=0,ae=rt.arsize();
	uint64_t const* nw64 = (const uint64_t*)nw;
	for (; ai != ae; ++ai,++nw64)
	{
		uint64_t val = ByteOrder<uint64_t>::ntoh( *nw64);
		rt.m_ar[ ai] = val;
	}
	return rt;
}

#define UINT64_CONST(hi,lo) (((uint64_t)hi << 32) | (uint64_t)lo)

uint64_t hash64Bitshuffle( uint64_t a)
{
	a = (a+UINT64_CONST(0x7ed55d16, 0x17ad327a)) + (a<<31);
	a = (a^UINT64_CONST(0xc761c23c, 0x384321a7)) ^ (a>>19);
	a = (a+UINT64_CONST(0x165667b1, 0x71b497a3)) + (a<<5);
	a = (a+UINT64_CONST(0xd3a2646c, 0x61a5cd01)) ^ (a<<9);
	a = (a+UINT64_CONST(0xfd7046c5, 0x29aa46c8)) + (a<<41);
	a = (a^UINT64_CONST(0xb55a4f09, 0x1a99cf51)) ^ (a>>17);
	a = (a+UINT64_CONST(0x19fa430a, 0x826cd104)) + (a<<7);
	a = (a^UINT64_CONST(0xc7812398, 0x5cfa1097)) ^ (a>>27);
	a = (a+UINT64_CONST(0x37af7627, 0x1ff18537)) ^ (a<<12);
	a = (a+UINT64_CONST(0xc16752fa, 0x0917283a)) + (a<<21);
	return a;
}

#define KNUTH_CONST 2654435761U
SimHash SimHash::randomHash( std::size_t size_, unsigned int seed)
{
	SimHash rt( size_, false);
	std::size_t ai=0,ae = arsize(size_);
	for (; ai != ae; ++ai)
	{
		rt.m_ar[ai] = hash64Bitshuffle( (seed + ai) * KNUTH_CONST);
	}
	return rt;
}



