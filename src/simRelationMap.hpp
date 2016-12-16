/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for storing similarity relations
#ifndef _STRUS_VECTOR_SIMILARITY_RELATION_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SIMILARITY_RELATION_MAP_HPP_INCLUDED
#include "simHash.hpp"
#include "simGroup.hpp"
#include "strus/base/stdint.h"
#include <vector>
#include <map>
#include <limits>

namespace strus {

/// \brief Structure for storing sample similarity relations
class SimRelationMap
{
public:
	/// \brief One column element in the sparse matrix of similarity relations
	struct Element
	{
		SampleIndex index;		///> index of the similar object
		unsigned short simdist;		///> edit distance describing similarity

		/// \brief Default constructor
		Element()
			:index(0),simdist(0){}
		/// \brief Constructor
		Element( SampleIndex index_, unsigned short simdist_)
			:index(index_),simdist(simdist_){}
		/// \brief Copy constructor
		Element( const Element& o)
			:index(o.index),simdist(o.simdist){}

		/// \brief Order with ascending similarity (closest first)
		bool operator < (const Element& o) const
		{
			if (simdist == o.simdist) return index < o.index;
			return simdist < o.simdist;
		}
	};

	static std::vector<Element> selectElementSubset( const std::vector<Element>& elemlist, unsigned int maxsimsam, unsigned int rndsimsam, unsigned int rndseed);
	/// \brief Merge two element lists
	static std::vector<Element> mergeElementLists( const std::vector<Element>& elemlist1, const std::vector<Element>& elemlist2);

public:
	/// \brief Default constructor
	SimRelationMap()
		:m_ar(),m_rowdescrmap(),m_startIndex(std::numeric_limits<SampleIndex>::max()),m_endIndex(0){}
	/// \brief Copy constructor
	SimRelationMap( const SimRelationMap& o)
		:m_ar(o.m_ar),m_rowdescrmap(o.m_rowdescrmap),m_startIndex(o.m_startIndex),m_endIndex(o.m_endIndex){}

	/// \brief Access structure for iterating on a row
	class Row
	{
	public:
		typedef std::vector<Element>::const_iterator const_iterator;

		/// \brief Constructor
		Row( const const_iterator& begin_, const const_iterator& end_)
			:m_begin(begin_),m_end(end_){}

		const_iterator begin() const	{return m_begin;}
		const_iterator end() const	{return m_end;}

		bool empty() const		{return m_begin == m_end;}

	private:
		const_iterator m_begin;
		const_iterator m_end;
	};

	/// \brief Get a row declared by index
	/// \param[in] index index of the row to access
	Row row( const SampleIndex& index) const
	{
		RowDescrMap::const_iterator ri = m_rowdescrmap.find( index);
		if (ri == m_rowdescrmap.end()) return Row( m_ar.end(), m_ar.end());
		return Row( m_ar.begin() + ri->second.aridx, m_ar.begin() + ri->second.aridx + ri->second.size);
	}

	/// \brief Get this map as string
	std::string tostring() const;

	/// \brief Add a row reclaring the samples related (similar) to a sample
	/// \param[in] index index of sample to declare related elements of
	/// \param[in] ar array of samples with similarity distance related to the element referenced by 'index'
	void addRow( const SampleIndex& index, const std::vector<Element>& ar);
	void addRow( const SampleIndex& index, std::vector<Element>::const_iterator ai, const std::vector<Element>::const_iterator& ae);

	/// \brief Get the total number of similarity relations detect until now
	std::size_t nofRelationsDetected() const
	{
		return m_ar.size();
	}

	SampleIndex endIndex() const
	{
		return m_endIndex;
	}
	SampleIndex startIndex() const
	{
		return (m_startIndex > m_endIndex) ? m_endIndex : m_startIndex;
	}

	/// \brief Get a similarity relation map containing all elements (b,a) for this containing (a,b)
	/// \remark Absolutely necessary to call this function after building a similarity relation map with (a,b) for a > b !
	SimRelationMap mirror() const;

	/// \brief Join two similarity relation maps to one without overlapping rows
	/// \param[in] o similarity relation map to join with
	/// \remark throws when rows of the two maps are not disjoint
	void join( const SimRelationMap& o);

	void clear()
	{
		m_ar.clear();
		m_rowdescrmap.clear();
		m_startIndex = std::numeric_limits<SampleIndex>::max();
		m_endIndex = 0;
	}

private:
	struct RowDescr
	{
		std::size_t aridx;
		std::size_t size;

		RowDescr()
			:aridx(0),size(0){}
		RowDescr( std::size_t aridx_, std::size_t size_)
			:aridx(aridx_),size(size_){}
		RowDescr( const RowDescr& o)
			:aridx(o.aridx),size(o.size){}
	};
	typedef std::map<SampleIndex,RowDescr> RowDescrMap;

private:
	std::vector<Element> m_ar;
	RowDescrMap m_rowdescrmap;
	SampleIndex m_startIndex;
	SampleIndex m_endIndex;
};

}//namespace
#endif
