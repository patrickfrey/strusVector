/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Element of a result of retrieval of the most similar LSH values
#ifndef _STRUS_VECTOR_SIMHASH_QUERY_RESULT_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_QUERY_RESULT_HPP_INCLUDED
#include "strus/index.hpp"

namespace strus {

class SimHashQueryResult
{
public:
	/// \brief Default constructor
	SimHashQueryResult()
		:m_featno(0),m_weight(0.0){}
	/// \brief Copy constructor
	SimHashQueryResult( const SimHashQueryResult& o)
		:m_featno(o.m_featno),m_weight(o.m_weight){}
	/// \brief Constructor
	SimHashQueryResult( const Index& featno_, double weight_)
		:m_featno(featno_),m_weight(weight_){}

	Index featno() const			{return m_featno;}
	double weight() const			{return m_weight;}

	void setWeight( double weight_)		{m_weight = weight_;}

	bool operator < ( const SimHashQueryResult& o) const
	{
		if (m_weight < o.m_weight) return true;
		if (m_weight > o.m_weight) return false;
		return m_featno < o.m_featno;
	}
	bool operator > ( const SimHashQueryResult& o) const
	{
		if (m_weight > o.m_weight) return true;
		if (m_weight < o.m_weight) return false;
		return m_featno > o.m_featno;
	}

private:
	Index m_featno;
	double m_weight;
};

}//namespace
#endif

