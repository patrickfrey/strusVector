/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for storing similarity group representants (individuals in the genetic algorithm for breeding similarity group representants)
#ifndef _STRUS_VECTOR_SPACE_MODEL_SIMILARITY_GROUP_LIST_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SIMILARITY_GROUP_LIST_HPP_INCLUDED
#include "simGroup.hpp"
#include "simHash.hpp"
#include "strus/base/stdint.h"
#include <list>

namespace strus {

/// \brief Structure for storing a representant of a similarity group
class SimGroupList
	:protected std::list<SimGroup>
{
public:
	typedef std::list<SimGroup> Parent;

public:
	explicit SimGroupList( unsigned int nofbits_)
		:Parent()
		,m_elementAllocator(nofbits_ / SimHash::NofElementBits + (nofbits_ % SimHash::NofElementBits != 0)?1:0)
		,m_nofbits(nofbits_){}

	SimGroupList( const SimGroupList& o)
		:Parent()
		,m_elementAllocator(o.m_nofbits / SimHash::NofElementBits + (o.m_nofbits % SimHash::NofElementBits != 0)?1:0)
		,m_nofbits(o.m_nofbits)
	{
		Parent::const_iterator oi = o.begin(), oe = o.end();
		for (; oi != oe; ++oi)
		{
			Parent::push_back( SimGroup( *oi, m_elementAllocator));
		}
	}
	void push_back( const SimGroup& element)
	{
		Parent::push_back( SimGroup( element, m_elementAllocator));
	}
	typedef Parent::const_iterator const_iterator;
	typedef Parent::iterator iterator;

	const_iterator begin() const		{return Parent::begin();}
	const_iterator end() const		{return Parent::end();}
	iterator begin()			{return Parent::begin();}
	iterator end()				{return Parent::end();}

	void erase( const iterator& itr)
	{
		itr->freeGenCode( m_elementAllocator);
		Parent::erase( itr);
	}

private:
	IntrusiveVectorCollection<uint64_t> m_elementAllocator;
	unsigned int m_nofbits;
};
}//namespace
#endif


