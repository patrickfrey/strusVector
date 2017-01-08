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
#include <iostream>
#include <sstream>

using namespace strus;

PageRank::PageId PageRank::getPageId( const std::string& id)
{
	IdMap::const_iterator fi = m_idmap.find( id);
	if (fi == m_midmap.end())
	{
		return m_idmap[ id] = m_idcnt++;
	}
	else
	{
		return fi->second;
	}
}

void PageRank::addLink( const std::string& from, const std::string& to)
{
	PageId from_idx = getPageId( from);
	PageId to_idx = getPageId( to);
	m_linkMatrix.insert( Link( from_idx, to_idx));
}

std::vector<PageWeight> PageRank::calculate() const
{
	LinkMatrix::const_iterator li = m_linkMatrix.begin(), le = m_linkMatrix.end();
	arma::umat locations;
	arma::vec values;
	arma::vec vv;

	for (; li != le; ++li)
	{
		locations << li->first;
	}
	locations << arma::endr;
	li = m_linkMatrix.begin();
	for (; li != le; ++li)
	{
		locations << li->second;
	}
	locations << arma::endr;
	li = m_linkMatrix.begin();
	while (li != le)
	{
		LinkMatrix::const_iterator ln = li;
		unsigned int linkcnt = 0;
		for (; ln != le && ln->first == li->first; ++ln,++linkcnt){}
		double weight = 1.0 / (double)linkcnt;
		for (; li != le && li->first == li->first; ++li)
		{
			values << weight;
		}
	}
	values << arma::endr;

	arma::vec ee;
	PageId vi = 0;
	for (; vi < m_idcnt; ++vi)
	{
		vv << (1.0 / (double)m_idcnt);
		ee << (1.0 / (double)m_idcnt);
	}

	arma::sp_mat M( locations, values);
	double dumpingFactor = 0.85;

	unsigned int ii=0, ie=NofIterations;
	for (; ii < ie; ++ie)
	{
		vv = M * vv * dumpingFactor + (1.0 - dumpingFactor) * ee;
	}
}


