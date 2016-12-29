/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of a page rank calculation 
#ifndef _STRUS_VECTOR_PAGERANK_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_PAGERANK_IMPLEMENTATION_HPP_INCLUDED
#include <string>
#include <map>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

/// \brief Simple, not very sophisticated implementation of a page rank calculation
/// \note This class is a calculation based on the patented page rank algorithm (http://www.google.com/patents/US6285999). In order to use it with Strus, you have to explicitely enable its building with the CMake flag -DWITH_PAGERANK="YES". Strus is built without page rank by default.
class PageRank
{
public:
	PageRank()
		:m_idcnt(0){}
	~PageRank(){}

	void addLink( const std::string& from, const std::string& to);

	class PageWeight
	{
	public:

	private:
		std::string id;
		double weight;
	};

private:
	typedef unsigned int PageId;
	PageId getPageId( const std::string& rid);

private:
	typedef std::pair<PageId,PageId> Link;
	typedef std::set<Link> LinkMatrix;
	typedef std::map<std::string,PageId> IdMap;

	LinkMatrix m_connectionMatrix;
	IdMap m_idmap;
	PageId m_idcnt;
};

} //namespace
#endif


