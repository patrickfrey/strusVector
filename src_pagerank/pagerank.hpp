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
#include <vector>
#include <map>
#include <set>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

/// \brief Simple, not very sophisticated implementation of a page rank calculation
/// \note This class is a calculation based on the patented page rank algorithm (http://www.google.com/patents/US6285999). In order to use it with Strus, you have to explicitely enable its building with the CMake flag -DWITH_PAGERANK="YES". Strus is built without page rank by default.
class PageRank
{
public:
	enum {NofIterations = 32};

	explicit PageRank( unsigned int nofIterations_=NofIterations, double dampeningFactor_ = 0.85)
		:m_idcnt(0),m_maxrow(0),m_maxcol(0),m_observed_item(0),m_nofIterations(nofIterations_),m_dampeningFactor(dampeningFactor_){}
	PageRank( const PageRank& o)
		:m_redirectMap(o.m_redirectMap)
		,m_linkMatrix(o.m_linkMatrix)
		,m_idmap(o.m_idmap)
		,m_idinv(o.m_idinv)
		,m_defset(o.m_defset)
		,m_idcnt(o.m_idcnt)
		,m_maxrow(o.m_maxrow)
		,m_maxcol(o.m_maxcol)
		,m_nofIterations(o.m_nofIterations)
		,m_dampeningFactor(o.m_dampeningFactor){}

	~PageRank(){}

	typedef unsigned int PageId;
	PageId getPageId( const std::string& name) const;
	PageId getOrCreatePageId( const std::string& name, bool isdef);
	std::string getPageName( const PageId& id) const;

	void addLink( const PageId& from, const PageId& to, unsigned int cnt=1);
	void defineRedirect( const PageId& from, const PageId& to)
	{
		if (from != to) m_redirectMap[ from] = to;
	}

	unsigned int nofPages() const
	{
		return m_idcnt;
	}

	std::vector<double> calculate() const;

	void printRedirectsToFile( const std::string& filename) const;

	PageRank reduce() const;

	/*[-]*/void declare_observed_item( const PageId& pid)
	{
		if (m_observed_item != 0 && m_observed_item != pid)
		{
			std::cerr << "+++ observed item does not match: " << m_observed_item << " != " << pid << std::endl;
		}
		else
		{
			m_observed_item = pid;
		}
	}

private:
	PageId resolveRedirect( const PageId& pid) const;

private:
	typedef std::pair<PageId,PageId> Link;
	typedef std::map<Link,unsigned int> LinkMatrix;
	typedef std::map<std::string,PageId> IdMap;
	typedef std::map<PageId,PageId> RedirectMap;

	RedirectMap m_redirectMap;
	LinkMatrix m_linkMatrix;
	IdMap m_idmap;
	std::vector<std::string> m_idinv;
	std::set<PageId> m_defset;
	PageId m_idcnt;
	PageId m_maxrow;
	PageId m_maxcol;
	PageId m_observed_item;
	unsigned int m_nofIterations;
	double m_dampeningFactor;
};

} //namespace
#endif


