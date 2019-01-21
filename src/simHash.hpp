/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Similarity hash structure
#ifndef _STRUS_VECTOR_SIMHASH_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "strus/index.hpp"
#include "internationalization.hpp"
#include <vector>
#include <string>
#include <utility>

namespace strus {

struct Functor_OR  {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa|bb;}};
struct Functor_AND {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa&bb;}};
struct Functor_XOR {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa^bb;}};
struct Functor_INV {static uint64_t call( uint64_t aa)			{return ~aa;}};

/// \brief Structure for the similarity fingerprint used
class SimHash
{
private:
	enum {NofElementBits=64};
	uint64_t* m_ar;
	int m_size;
	Index m_id;
public:
	SimHash()
		:m_ar(0),m_size(0),m_id(0){}
	SimHash( const SimHash& o);
	SimHash( const std::vector<bool>& bv, const Index& id_);
	SimHash( int size_, bool initval, const Index& id_);
	~SimHash();
#if __cplusplus >= 201103L
	SimHash( SimHash&& o) :m_ar(o.m_ar),m_size(o.m_size),m_id(o.m_id) {o.m_ar=0;o.m_size=0;}
	//SimHash& operator=( SimHash&& o) {if (m_ar) std::free(m_ar); m_ar=std::move(o.m_ar); m_size=o.m_size; m_id=o.m_id; return *this;}
#endif
	/// \brief Get element value by index
	bool operator[]( int idx) const;
	/// \brief Get next 0 value index after idx
	int next_free( int idx) const;
	/// \brief Set element with index to value
	void set( int idx, bool value);
	/// \brief Calculate distance (bits with different value)
	int dist( const SimHash& o) const;
	/// \brief Calculate the number of bits set to 1
	int count() const;
	/// \brief Test if the distance is smaller than a given dist
	bool near( const SimHash& o, int dist) const;
	/// \brief Get all indices of elements set to 1 or 0 (defined by parameter)
	std::vector<int> indices( bool what) const;

	void setId( const Index& id_)
	{
		m_id = id_;
	}

	SimHash& operator=( const SimHash& o);
	bool operator==( const SimHash& o) const	{return compare(o) == 0;}
	bool operator!=( const SimHash& o) const	{return compare(o) != 0;}
	bool operator<( const SimHash& o) const		{return compare(o) < 0;}
	bool operator<=( const SimHash& o) const	{return compare(o) <= 0;}
	bool operator>( const SimHash& o) const		{return compare(o) > 0;}
	bool operator>=( const SimHash& o) const	{return compare(o) >= 0;}

private:
	int compare( const SimHash& o) const;

	template <class Functor>
	SimHash BINOP( const SimHash& o) const
	{
		if (size() != o.size()) throw std::runtime_error( _TXT("binary sim hash operation on incompatible operands"));
		SimHash rt( size(), false, id());
		uint64_t* ri = rt.m_ar;
		uint64_t const* ai = m_ar; const uint64_t* ae = m_ar + arsize();
		uint64_t const* oi = o.m_ar;
		for (; ai != ae; ++ri,++ai,++oi) *ri = Functor::call( *ai, *oi);
		return rt;
	}
	template <class Functor>
	SimHash& BINOP_ASSIGN( const SimHash& o)
	{
		if (size() != o.size()) throw std::runtime_error( _TXT("binary sim hash operation on incompatible operands"));
		uint64_t* ai = m_ar; uint64_t* ae = m_ar + arsize();
		uint64_t const* oi = o.m_ar;
		for (; ai != ae; ++ai,++oi) *ai = Functor::call( *ai, *oi);
		return *this;
	}
	template <class Functor>
	SimHash UNOP() const
	{
		SimHash rt( m_size, false, id());
		uint64_t* ri = rt.m_ar;
		uint64_t const* ai = m_ar; const uint64_t* ae = m_ar + arsize();
		for (; ai != ae; ++ri,++ai) *ri = Functor::call( *ai);
		return rt;
	}
public:

	/// \brief Binary XOR
	SimHash operator ^ ( const SimHash& o) const	{return BINOP<Functor_XOR>( o);}
	/// \brief Binary AND
	SimHash operator & ( const SimHash& o) const	{return BINOP<Functor_AND>( o);}
	/// \brief Binary OR
	SimHash operator | ( const SimHash& o) const	{return BINOP<Functor_OR>( o);}
	/// \brief Unary negation
	SimHash operator ~ () const			{return UNOP<Functor_INV>();}
	/// \brief Assignment XOR
	SimHash& operator ^= ( const SimHash& o)	{return BINOP_ASSIGN<Functor_XOR>( o);}
	/// \brief Assignment AND
	SimHash& operator &= ( const SimHash& o)	{return BINOP_ASSIGN<Functor_AND>( o);}
	/// \brief Assignment OR
	SimHash& operator |= ( const SimHash& o)	{return BINOP_ASSIGN<Functor_OR>( o);}

	/// \brief Get the bit field as string of "0" and "1"
	std::string tostring() const;
	/// \brief Number of bits represented
	int size() const				{return m_size;}
	/// \brief Identifier of the vector represented
	Index id() const				{return m_id;}
	/// \brief Evaluate if is defined or empty
	bool defined() const				{return !!m_ar;}

	/// \brief Create a randomized SimHash of a given size
	static SimHash randomHash( int size_, int seed, const Index& id_);
	/// \brief Serialize
	std::string serialization() const;
	/// \brief Deserialize
	static SimHash fromSerialization( const char* in, int insize);
	/// \brief Deserialize
	static SimHash fromSerialization( const std::string& blob);

	const uint64_t* ar() const			{return m_ar;}
	/// \brief Get the size of the array used to represent the sim hash value
	int arsize() const			{return (m_size+NofElementBits-1)/NofElementBits;}
	/// \brief Get the size of the array used to represent the sim hash value
	static int arsize( int size_)		{return (size_+NofElementBits-1)/NofElementBits;}

};

}//namespace
#endif


