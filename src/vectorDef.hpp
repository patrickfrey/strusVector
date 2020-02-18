/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Definition of a vector with id
#ifndef _STRUS_VECTOR_DEF_HPP_INCLUDED
#define _STRUS_VECTOR_DEF_HPP_INCLUDED
#include "strus/storage/index.hpp"
#include "strus/storage/wordVector.hpp"
#include "simHash.hpp"
#include <vector>

namespace strus {

/// \brief Vector definition with associated id 
class VectorDef
{
public:
	explicit VectorDef( const Index& id_)
		:m_vec(),m_lsh(),m_id(id_){}
	VectorDef( const WordVector& vec_, const SimHash& lsh_, const Index& id_)
		:m_vec(vec_),m_lsh(lsh_),m_id(id_){}
	VectorDef( const VectorDef& o)
		:m_vec(o.m_vec),m_lsh(o.m_lsh),m_id(o.m_id){}

	const WordVector& vec() const	{return m_vec;}
	const SimHash& lsh() const	{return m_lsh;}
	const Index& id() const		{return m_id;}

	void setId( const Index& id_)
	{
		m_id = id_;
		m_lsh.setId( id_);
	}

private:
	WordVector m_vec;
	SimHash m_lsh;
	Index m_id;
};

}//namespace
#endif


