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
#include <string>
#include <iostream>
#include <sstream>
#include <limits>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace strus;

SimHash::SimHash( std::size_t nofbits_, bool initval, Collection& collection)
	:Parent(collection.newVector()),m_nofbits(nofbits_)
{
	if (m_nofbits == 0) return;
	std::size_t allelem = collection.vecsize() * NofElementBits;
	if (m_nofbits > allelem) throw strus::runtime_error(_TXT("sim hash number of elements out of range"));
	std::size_t fillelem = (collection.vecsize()-1) * NofElementBits;
	if (m_nofbits <= fillelem) throw strus::runtime_error(_TXT("sim hash number of elements out of range"));
	std::size_t restelem = m_nofbits - fillelem;
	if (restelem == NofElementBits)
	{
		restelem = 0;
	}

	uint64_t elem = (initval)?std::numeric_limits<uint64_t>::max():0;
	Parent::iterator ai = Parent::begin(), ae = Parent::end();
	if (restelem)
	{
		for (--ae; ai != ae; ++ai) *ai = elem;
		elem <<= (NofElementBits - restelem);
		*ai = elem;
	}
	else
	{
		for (; ai != ae; ++ai) *ai = elem;
	}
}

SimHash::SimHash( const std::vector<bool>& bv, Collection& collection)
	:Parent(collection.newVector()),m_nofbits(bv.size())
{
	if (bv.empty()) return;
	uint64_t elem = 0;
	std::vector<bool>::const_iterator bi = bv.begin(), be = bv.end();
	Parent::iterator ai = Parent::begin();
	unsigned int bidx = 0;
	for (; bi != be; ++bi,++bidx)
	{
		if (bidx == (int)NofElementBits)
		{
			*ai++ = elem;
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
	*ai = elem;
}

bool SimHash::operator[]( std::size_t idx) const
{
	std::size_t aridx = idx / NofElementBits;
	std::size_t arofs = idx % NofElementBits;
	if (aridx >= Parent::size()) return false;
	uint64_t mask = 1;
	mask <<= (NofElementBits-1 - arofs);
	return (Parent::operator[]( aridx) & mask) != 0;
}

void SimHash::set( std::size_t idx, bool value)
{
	std::size_t aridx = idx / NofElementBits;
	std::size_t arofs = idx % NofElementBits;
	if (aridx >= Parent::size())
	{
		throw strus::runtime_error(_TXT("array bound write in %s"), "SimHash");
	}
	uint64_t mask = 1;
	mask <<= (NofElementBits-1 - arofs);
	if (value)
	{
		Parent::operator[]( aridx) |= mask;
	}
	else
	{
		Parent::operator[]( aridx) &= ~mask;
	}
}

std::vector<std::size_t> SimHash::indices( bool what) const
{
	std::vector<std::size_t> rt;
	Parent::const_iterator ai = Parent::begin(), ae = Parent::end();
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
	Parent::const_iterator ai = Parent::begin(), ae = Parent::end();
	for (; ai != ae; ++ai)
	{
		rt += strus::BitOperations::bitCount( *ai);
	}
	return rt;
}

unsigned int SimHash::dist( const SimHash& o) const
{
	unsigned int rt = 0;
	Parent::const_iterator ai = Parent::begin(), ae = Parent::end();
	Parent::const_iterator oi = o.begin(), oe = o.end();
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
	Parent::const_iterator ai = Parent::begin(), ae = Parent::end();
	Parent::const_iterator oi = o.begin(), oe = o.end();
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
	Parent::const_iterator ai = Parent::begin(), ae = Parent::end();
	for (unsigned int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (aidx) rt << '|';
		uint64_t mi = 1;
		unsigned int cnt = m_nofbits - (aidx * NofElementBits);
		mi <<= (NofElementBits-1);
		for (; mi && cnt; mi >>= 1, --cnt)
		{
			rt << ((*ai & mi)?'1':'0');
		}
	}
	return rt.str();
}

void SimHashCollection::printSerialization( std::string& out, const SimHashCollection& ar)
{
	uint32_t nw;
	nw = htonl( (uint32_t)ar.nofbits());	out.append( (const char*)&nw, sizeof(nw));
	nw = htonl( (uint32_t)ar.size());	out.append( (const char*)&nw, sizeof(nw));
	const_iterator vi = ar.begin(), ve = ar.end();
	for (; vi != ve; ++vi)
	{
		SimHash::const_iterator ai = vi->begin(), ae = vi->end();
		for (; ai != ae; ++ai)
		{
			nw = htonl( *ai >> 32);		out.append( (const char*)&nw, sizeof(nw));
			nw = htonl( *ai & 0xFFffFFffU);	out.append( (const char*)&nw, sizeof(nw));
		}
	}
}

SimHashCollection SimHashCollection::createFromSerialization( const std::string& in, std::size_t& itr)
{
	uint32_t const* nw = (const uint32_t*)(void*)(in.c_str() + itr);
	const uint32_t* start = nw;
	std::size_t nofbits = ntohl( *nw++);
	std::size_t arsize = ntohl( *nw++);
	SimHashCollection rt( nofbits);

	std::size_t vi=0,ve = arsize;
	for (; vi < ve; ++vi)
	{
		SimHash elem( nofbits, false, rt);
		SimHash::iterator ri = elem.begin(), re = elem.end();
		for (; ri != re; ++ri)
		{
			uint64_t val = ntohl( *nw++);
			val = (val << 32) | ntohl( *nw++);
			*ri = val;
		}
		rt.push_back_own( elem);
	}
	itr += (nw - start) * sizeof(uint32_t);
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

SimHash SimHash::randomHash( std::size_t nofbits_, unsigned int seed, Collection& collection)
{
	SimHash rt( nofbits_, false, collection);
	iterator ri = rt.begin(), re = rt.end();
	enum {KnuthConst=2654435761};
	std::size_t restelem = nofbits_ % NofElementBits;
	if (restelem)
	{
		unsigned int ridx=0;
		for (--re; ri != re; ++ri,++ridx)
		{
			*ri = hash64Bitshuffle( (seed + ridx) * KnuthConst);
		}
		*ri = hash64Bitshuffle( (seed + ridx) * KnuthConst) << (NofElementBits - restelem);
	}
	else
	{
		unsigned int ridx=0;
		for (; ri != re; ++ri,++ridx)
		{
			*ri = hash64Bitshuffle( (seed + ridx) * KnuthConst);
		}
	}
	return rt;
}



