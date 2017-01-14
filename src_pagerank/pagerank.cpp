/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of a page rank calculation 
#include "pagerank.hpp"
#include "armadillo"

using namespace strus;

PageRank::PageId PageRank::getPageId( const std::string& name) const
{
	IdMap::const_iterator fi = m_idmap.find( name);
	if (fi == m_idmap.end())
	{
		return 0;
	}
	else
	{
		return fi->second;
	}
}

std::string PageRank::getPageName( const PageId& id) const
{
	if (id == 0 || id > m_idcnt) throw std::runtime_error("illegal page id value (addLink)");
	return m_idinv[ id-1];
}

PageRank::PageId PageRank::getOrCreatePageId( const std::string& name)
{
	IdMap::const_iterator fi = m_idmap.find( name);
	if (fi == m_idmap.end())
	{
		m_idinv.push_back( name);
		return m_idmap[ name] = ++m_idcnt;
	}
	else
	{
		return fi->second;
	}
}

void PageRank::addLink( const PageId& from, const PageId& to)
{
	if (from == 0 || from > m_idcnt) throw std::runtime_error("illegal page id value (addLink)");
	if (to == 0 || to > m_idcnt) throw std::runtime_error("illegal page id value (addLink)");
	m_linkMatrix.insert( Link( from-1, to-1));
	if (from > m_maxrow) m_maxrow = from;
}

std::vector<double> PageRank::calculate() const
{
	if (m_idcnt == 0) return std::vector<double>();
	LinkMatrix::const_iterator li = m_linkMatrix.begin(), le = m_linkMatrix.end();

	arma::umat locations = arma::zeros<arma::umat>( 2, m_linkMatrix.size() + ((m_maxrow < m_idcnt) ? 1:0));
	for (unsigned int lidx=0; li != le; ++li,++lidx)
	{
		locations( 1, lidx) = li->first;
		locations( 0, lidx) = li->second;
	}
	arma::vec values( m_linkMatrix.size()  + ((m_maxrow < m_idcnt) ? 1:0));
	li = m_linkMatrix.begin();
	unsigned int lidx=0;
	while (li != le)
	{
		LinkMatrix::const_iterator ln = li;
		unsigned int linkcnt = 0;
		for (; ln != le && ln->first == li->first; ++ln,++linkcnt){}
		double weight = 1.0 / (double)linkcnt;
		for (; li != ln; ++li,++lidx)
		{
			values( lidx) = weight;
		}
	}
	if (m_maxrow < m_idcnt)
	{
		locations( 1, m_linkMatrix.size()) = m_idcnt-1;
		locations( 0, m_linkMatrix.size()) = 0;
		values( m_linkMatrix.size()) = 0.0;
	}
	std::vector<double> vv_;
	std::vector<double> ee_;
	PageId vi = 0;
	for (; vi < m_idcnt; ++vi)
	{
		vv_.push_back( (1.0 / (double)m_idcnt));
		ee_.push_back( (1.0 / (double)m_idcnt));
	}
	arma::vec ee( ee_);
	arma::vec vv( vv_);

	arma::sp_mat M( locations, values);

	unsigned int ii=0, ie=m_nofIterations;
	for (; ii < ie; ++ii)
	{
		vv = M * vv * m_dampeningFactor + (1.0 - m_dampeningFactor) * ee;
	}
	std::vector<double> rt;
	rt.reserve( vv.size());
	rt.insert( rt.end(), vv.begin(), vv.end());
	return rt;
}


