/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LSH Similarity model structure
#ifndef _STRUS_VECTOR_LSH_MODEL_HPP_INCLUDED
#define _STRUS_VECTOR_LSH_MODEL_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "simHash.hpp"
#include "armadillo"
#include <vector>
#include <string>

namespace strus {

/// \brief Structure for calculating LSH similarity
class LshModel
{
public:
	/// \brief Constructor
	LshModel();
	LshModel( int vecdim_, int bits_, int variations_, int simdist_, int probsimdist_);
	LshModel( const LshModel& o);

	/// \brief map contents to string in readable form
	std::string tostring() const;

	/// \brief Serialize
	std::string serialization() const;
	/// \brief Deserialize
	static LshModel fromSerialization( const std::string& blob);
	/// \brief Deserialize
	static LshModel fromSerialization( const char* blob, std::size_t blobsize);

	/// \brief Calculate similarity hash of a vector
	/// \param[in] vec input vector
	/// \return simhash value acording to this model
	SimHash simHash( const arma::fvec& vec, const Index& id_) const;

	bool isequal( const LshModel& o) const;

	int vecdim() const
	{
		return m_vecdim;
	}
	int bits() const
	{
		return m_bits;
	}
	int variations() const
	{
		return m_variations;
	}
	int simdist() const
	{
		return m_simdist;
	}
	int probsimdist() const
	{
		return m_probsimdist;
	}
	void set_simdist( int value)
	{
		m_simdist = value;
	}
	void set_probsimdist( int value)
	{
		m_probsimdist = value;
	}
	int vectorBits() const
	{
		return m_bits * m_variations;
	}

private:
	static arma::fmat createModelMatrix( int vecdim_, int bits_);
	LshModel( int vecdim_, int bits_, int variations_, int simdist_, int probsimdist_, const arma::fmat& modelMatrix_, const std::vector<arma::fmat>& rotations_);

private:
	int m_vecdim;
	int m_bits;
	int m_variations;
	int m_simdist;
	int m_probsimdist;
	arma::fmat m_modelMatrix;
	std::vector<arma::fmat> m_rotations;
};

}//namespace
#endif


