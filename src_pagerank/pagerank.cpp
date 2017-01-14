/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of a page rank calculation 
#include "pagerank.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/fileio.hpp"
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

void PageRank::addLink( const PageId& from, const PageId& to, unsigned int cnt)
{
	if (from == 0 || from > m_idcnt) throw std::runtime_error("illegal page id value (addLink)");
	if (to == 0 || to > m_idcnt) throw std::runtime_error("illegal page id value (addLink)");
	Link lnk( from-1, to-1);
	LinkMatrix::iterator li = m_linkMatrix.find( lnk);
	if (li == m_linkMatrix.end())
	{
		m_linkMatrix[ lnk] = cnt;
	}
	else
	{
		li->second += cnt;
	}
	if (from > m_maxrow) m_maxrow = from;
	if (to > m_maxcol) m_maxcol = to;
}

std::vector<double> PageRank::calculate() const
{
	if (m_idcnt == 0) return std::vector<double>();
	LinkMatrix::const_iterator li = m_linkMatrix.begin(), le = m_linkMatrix.end();

	unsigned int nofDummyElements = ((m_maxrow < m_idcnt) ? 1:0) + ((m_maxcol < m_idcnt) ? 1:0);
	/*[-]*/std::cout << "++++ nofDummyElements " << nofDummyElements << std::endl;
	/*[-]*/std::cout << "++++ m_maxrow " << m_maxrow << std::endl;
	/*[-]*/std::cout << "++++ m_maxcol " << m_maxcol << std::endl;
	/*[-]*/std::cout << "++++ m_idcnt " << m_idcnt << std::endl;

	arma::umat locations = arma::zeros<arma::umat>( 2, m_linkMatrix.size() + nofDummyElements);
	for (unsigned int lidx=0; li != le; ++li,++lidx)
	{
		locations( 1, lidx) = li->first.first;
		locations( 0, lidx) = li->first.second;
	}
	arma::vec values( m_linkMatrix.size() + nofDummyElements);
	li = m_linkMatrix.begin();
	unsigned int lidx=0;
	while (li != le)
	{
		LinkMatrix::const_iterator ln = li;
		unsigned int linkcnt = 0;
		for (; ln != le && ln->first.first == li->first.first; ++ln, linkcnt += li->second){}
		double weight = 1.0 / (double)linkcnt;
		for (; li != ln; ++li,++lidx)
		{
			values( lidx) = li->second * weight;
		}
	}
	if (m_maxrow < m_idcnt)
	{
		locations( 1, m_linkMatrix.size()) = m_idcnt-1;
		locations( 0, m_linkMatrix.size()) = 0;
		values( m_linkMatrix.size()) = 0.0;
	}
	if (m_maxcol < m_idcnt)
	{
		locations( 1, m_linkMatrix.size() +nofDummyElements -1) = 0;
		locations( 0, m_linkMatrix.size() +nofDummyElements -1) = m_idcnt-1;
		values( m_linkMatrix.size() +nofDummyElements -1) = 0.0;
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

void PageRank::printRedirectsToFile( const std::string& filename) const
{
	enum {NofLinesPerChunk=100000};
	std::string outbuf;
	RedirectMap::const_iterator ri = m_redirectMap.begin(), re = m_redirectMap.end();
	for (unsigned int ridx=0; ri != re; ++ri,++ridx)
	{
		if (ridx >= NofLinesPerChunk)
		{
			unsigned int ec = appendFile( filename, outbuf);
			if (ec) throw std::runtime_error( string_format( "failed to print redirects to file: %s", ::strerror(ec)));
			ridx = 0;
			outbuf.clear();
		}
		outbuf.append( getPageName( ri->first));
		outbuf.push_back( '\t');
		outbuf.append( getPageName( ri->second));
		outbuf.push_back( '\n');
	}
	if (!outbuf.empty())
	{
		unsigned int ec = appendFile( filename, outbuf);
		if (ec) throw std::runtime_error( string_format( "failed to print redirects to file: %s", ::strerror(ec)));
	}
}


PageRank::PageId PageRank::resolveRedirect( const PageId& pid) const
{
	PageId rt = pid;
	PageId minimum = pid;
	RedirectMap::const_iterator ri = m_redirectMap.find( rt);
	while (ri != m_redirectMap.end() && ri->second != pid)
	{
		rt = ri->second;
		if (rt < minimum) minimum = rt;
		ri = m_redirectMap.find( rt);
	}
	if (ri->second == pid)
	{
		//... circular reference, take the smallest of all entries found in the circle
		return minimum;
	}
	return rt;
}

PageRank PageRank::reduce() const
{
	PageRank rt;
	LinkMatrix newLinkMatrix;
	std::set<PageId> definedset;
	LinkMatrix::const_iterator li = m_linkMatrix.begin(), le = m_linkMatrix.end();
	for (; li != le; ++li)
	{
		Link newlink = Link( 
				resolveRedirect( li->first.first), 
				resolveRedirect( li->first.second));

		LinkMatrix::iterator ni = newLinkMatrix.find( newlink);
		if (ni == newLinkMatrix.end())
		{
			newLinkMatrix[ newlink] = li->second;
			definedset.insert( newlink.first);
		}
		else
		{
			ni->second += li->second;
		}
	}
	LinkMatrix::const_iterator ni = newLinkMatrix.begin(), ne = newLinkMatrix.end();
	for (; ni != ne; ++ni)
	{
		if (definedset.find(ni->first.second) != definedset.end())
		{
			PageId fromid = rt.getOrCreatePageId( m_idinv[ ni->first.first]);
			PageId toid = rt.getOrCreatePageId( m_idinv[ ni->first.second]);
			unsigned int cnt = ni->second;
			rt.addLink( fromid, toid, cnt);
		}
	}
	return rt;
}

