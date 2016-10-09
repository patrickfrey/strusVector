/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Similarity hash structure
#ifndef _STRUS_VECTOR_SPACE_MODEL_SIMHASH_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SIMHASH_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "intrusiveVector.hpp"
#include <vector>
#include <string>

namespace strus {

struct Functor_OR  {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa|bb;}};
struct Functor_AND {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa&bb;}};
struct Functor_XOR {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa^bb;}};
struct Functor_INV {static uint64_t call( uint64_t aa)			{return ~aa;}};

/// \brief Structure for the similarity fingerprint used
class SimHash
	:public IntrusiveVector<uint64_t>
{
public:
	typedef IntrusiveVector<uint64_t> Parent;
	typedef IntrusiveVectorCollection<uint64_t> Collection;

public:
	SimHash()
		:Parent(),m_nofbits(0){}
	SimHash( const SimHash& o)
		:Parent(o),m_nofbits(o.m_nofbits){}
	SimHash( const std::vector<bool>& bv, Collection& collection);
	SimHash( std::size_t nofbits_, bool initval, Collection& collection);
	SimHash( const SimHash& o, Collection& collection)
		:Parent(collection.newVectorCopy(o)),m_nofbits(o.m_nofbits)
	{}

	/// \brief Get element value by index
	bool operator[]( std::size_t idx) const;
	/// \brief Set element with index to value
	void set( std::size_t idx, bool value);
	/// \brief Calculate distance (bits with different value)
	unsigned int dist( const SimHash& o) const;
	/// \brief Calculate the number of bits set to 1
	unsigned int count() const;
	/// \brief Test if the distance is smaller than a given dist
	bool near( const SimHash& o, unsigned int dist) const;
	/// \brief Get all indices of elements set to 1 or 0 (defined by parameter)
	std::vector<std::size_t> indices( bool what) const;

	SimHash& operator=( const SimHash& o)
	{
		assignContentCopy( o);
		return *this;
	}

private:
	template <class Functor>
	SimHash BINOP( const SimHash& o, Collection& collection) const
	{
		if (o.m_nofbits != m_nofbits) throw strus::runtime_error(_TXT("simhash binary operation with incompatible arguments"));
		SimHash rt( m_nofbits, false, collection);
		Parent::const_iterator ai = begin(), ae = end();
		Parent::const_iterator oi = o.begin();
		Parent::iterator ri = rt.begin();
		for (; ai != ae; ++ri,++ai,++oi) *ri = Functor::call( *ai, *oi);
		return rt;
	}
	template <class Functor>
	SimHash& BINOP_ASSIGN( const SimHash& o, Collection& collection)
	{
		if (o.m_nofbits != m_nofbits) throw strus::runtime_error(_TXT("simhash binary operation with incompatible arguments"));
		Parent::iterator ai = Parent::begin(), ae = Parent::end();
		Parent::const_iterator oi = o.Parent::begin();
		for (; ai != ae; ++ai,++oi) *ai = Functor::call( *ai, *oi);
		return *this;
	}
	template <class Functor>
	SimHash UNOP( Collection& collection) const
	{
		SimHash rt( m_nofbits, false, collection);
		Parent::iterator ri = rt.begin();
		Parent::const_iterator ai = Parent::begin(), ae = Parent::end();
		for (; ai != ae; ++ri,++ai) *ri = Functor::call( *ai);
		return rt;
	}
public:
	/// \brief Binary XOR
	SimHash XOR( const SimHash& o, Collection& collection) const	{return BINOP<Functor_XOR>( o, collection);}
	/// \brief Binary AND
	SimHash AND( const SimHash& o, Collection& collection) const	{return BINOP<Functor_AND>( o, collection);}
	/// \brief Binary OR
	SimHash OR( const SimHash& o, Collection& collection) const	{return BINOP<Functor_OR>( o, collection);}
	/// \brief Unary negation
	SimHash INV( Collection& collection) const			{return UNOP<Functor_INV>( collection);}
	/// \brief Assignment XOR
	SimHash& assign_XOR( const SimHash& o, Collection& collection)	{return BINOP_ASSIGN<Functor_XOR>( o, collection);}
	/// \brief Assignment AND
	SimHash& assign_AND( const SimHash& o, Collection& collection)	{return BINOP_ASSIGN<Functor_AND>( o, collection);}
	/// \brief Assignment OR
	SimHash& assign_OR( const SimHash& o, Collection& collection)	{return BINOP_ASSIGN<Functor_OR>( o, collection);}

	/// \brief Get the bit field as string of "0" and "1"
	std::string tostring() const;
	/// \brief Number of bits represented
	std::size_t nofbits() const					{return m_nofbits;}

	/// \brief Create a randomized SimHash of a given size
	static SimHash randomHash( std::size_t nofbits_, unsigned int seed, Collection& collection);

	enum {NofElementBits=64};

private:
	std::size_t m_nofbits;
};

typedef IntrusiveVectorCollection<uint64_t> SimHashAllocator;

class SimHashCollection
	:protected IntrusiveVectorCollection<uint64_t>
{
public:
	typedef IntrusiveVectorCollection<uint64_t> Parent;
public:
	explicit SimHashCollection( std::size_t nofbits_)
		:Parent(
			nofbits_ / SimHash::NofElementBits
			+ (nofbits_ % SimHash::NofElementBits != 0)?1:0,
			NofBlockElements)
		,m_nofbits(nofbits_){}
	SimHashCollection( const SimHashCollection& o)
		:Parent(
			o.m_nofbits / SimHash::NofElementBits
			+ (o.m_nofbits % SimHash::NofElementBits != 0)?1:0,
			NofBlockElements)
		 ,m_nofbits(o.m_nofbits)
	{
		const_iterator oi = o.begin(), oe = o.end();
		for (; oi != oe; ++oi)
		{
			m_ar.push_back( SimHash( *oi, *this));
		}
	}

	void push_back( const SimHash& element)
	{
		m_ar.push_back( SimHash( element, *this));
	}
	void push_back_own( const SimHash& element)
	{
		m_ar.push_back( element);
	}

	const SimHash& operator[]( std::size_t idx) const	{return m_ar[ idx];}
	SimHash& operator[]( std::size_t idx)			{return m_ar[ idx];}

	typedef std::vector<SimHash>::const_iterator const_iterator;
	typedef std::vector<SimHash>::iterator iterator;

	const_iterator begin() const				{return m_ar.begin();}
	const_iterator end() const				{return m_ar.end();}
	iterator begin()					{return m_ar.begin();}
	iterator end()						{return m_ar.end();}

	std::size_t nofbits() const				{return m_nofbits;}
	std::size_t size() const				{return m_ar.size();}
	std::size_t vecsize() const				{return Parent::vecsize();}

	/// \brief Serialize
	static void printSerialization( std::string& out, const SimHashCollection& ar);
	/// \brief Deserialize
	static SimHashCollection createFromSerialization( const std::string& in, std::size_t& itr);

private:
	enum {NofBlockElements=128};
	std::vector<SimHash> m_ar;
	std::size_t m_nofbits;
};

}//namespace
#endif


