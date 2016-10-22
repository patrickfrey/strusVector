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
#include "internationalization.hpp"
#include <vector>
#include <string>

namespace strus {

struct Functor_OR  {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa|bb;}};
struct Functor_AND {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa&bb;}};
struct Functor_XOR {static uint64_t call( uint64_t aa, uint64_t bb)	{return aa^bb;}};
struct Functor_INV {static uint64_t call( uint64_t aa)			{return ~aa;}};

/// \brief Structure for the similarity fingerprint used
class SimHash
{
public:
	SimHash()
		:m_ar(0),m_size(0){}
	SimHash( const SimHash& o);
	SimHash( const std::vector<bool>& bv);
	SimHash( std::size_t size_, bool initval);
	~SimHash();

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

	SimHash& operator=( const SimHash& o);

private:
	template <class Functor>
	SimHash BINOP( const SimHash& o) const
	{
		if (size() != o.size()) throw strus::runtime_error(_TXT("binary sim hash operation on incompatible operands"));
		SimHash rt( size(), false);
		uint64_t* ri = rt.m_ar;
		uint64_t const* ai = m_ar; const uint64_t* ae = m_ar + arsize();
		uint64_t const* oi = o.m_ar;
		for (; ai != ae; ++ri,++ai,++oi) *ri = Functor::call( *ai, *oi);
		return rt;
	}
	template <class Functor>
	SimHash& BINOP_ASSIGN( const SimHash& o)
	{
		if (size() != o.size()) throw strus::runtime_error(_TXT("binary sim hash operation on incompatible operands"));
		uint64_t* ai = m_ar; uint64_t* ae = m_ar + arsize();
		uint64_t const* oi = o.m_ar;
		for (; ai != ae; ++ai,++oi) *ai = Functor::call( *ai, *oi);
		return *this;
	}
	template <class Functor>
	SimHash UNOP() const
	{
		SimHash rt( m_size, false);
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
	std::size_t size() const			{return m_size;}

	/// \brief Create a randomized SimHash of a given size
	static SimHash randomHash( std::size_t size_, unsigned int seed);
	/// \brief Serialize
	std::string serialization() const;
	/// \brief Deserialize
	static SimHash createFromSerialization( const std::string& blob);

	/// \brief Get the size of the array used to represent the sim hash value
	std::size_t arsize() const			{return (m_size+NofElementBits-1)/NofElementBits;}
	/// \brief Get the size of the array used to represent the sim hash value
	static std::size_t arsize( unsigned int size_)	{return (size_+NofElementBits-1)/NofElementBits;}

private:
	enum {NofElementBits=64};
	uint64_t* m_ar;
	unsigned int m_size;
};

}//namespace
#endif


