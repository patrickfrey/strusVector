/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LSH Similarity model structure
#ifndef _STRUS_VECTOR_SPACE_MODEL_LSHMODEL_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_LSHMODEL_HPP_INCLUDED
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
	LshModel( std::size_t dim_, std::size_t nofbits_, std::size_t variations_);
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
	SimHash simHash( const arma::vec& vec) const;

	bool isequal( const LshModel& o) const;

private:
	static arma::mat createModelMatrix( std::size_t dim_, std::size_t nofbits_);
	LshModel( std::size_t dim_, std::size_t nofbits_, std::size_t variations_, const arma::mat& modelMatrix_, const std::vector<arma::mat>& rotations_);

private:
	std::size_t m_dim;
	std::size_t m_nofbits;
	std::size_t m_variations;
	arma::mat m_modelMatrix;
	std::vector<arma::mat> m_rotations;
};

}//namespace
#endif


