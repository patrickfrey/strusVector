/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of a page rank calculation 
#include "pagerank.hpp"

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
	m_connectionMatrix.insert( Link( from_idx, to_idx));
}


