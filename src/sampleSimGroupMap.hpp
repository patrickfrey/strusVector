/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for a map of sample indices to similarity groups they are members of
#ifndef _STRUS_VECTOR_SPACE_MODEL_SAMPLE_TO_SIM_GROUP_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SAMPLE_TO_SIM_GROUP_MAP_HPP_INCLUDED
#include "simGroup.hpp"
#include <map>
#include <cstring>

namespace strus {

/// \brief Structure for storing sample to group relations
class SampleSimGroupMap
{
public:
	explicit SampleSimGroupMap( std::size_t nofNodes_, std::size_t maxNodeSize_);
	SampleSimGroupMap( const SampleSimGroupMap& o);
	~SampleSimGroupMap();

	void check() const;
	bool insert( const std::size_t& ndidx, const ConceptIndex& groupidx)
		{return m_nodear[ndidx].insert( groupidx, m_maxnodesize);}
	/// \brief Evaluate if a sample references a group
	bool contains( const std::size_t& ndidx, const ConceptIndex& groupidx) const
		{return m_nodear[ndidx].contains( groupidx);}
	/// \brief Evaluate if the two samples share a group reference
	bool shares( const std::size_t& ndidx1, const std::size_t& ndidx2) const;
	/// \brief Remove sample reference relationship to a group
	bool remove( const std::size_t& ndidx, const ConceptIndex& groupidx)
		{return m_nodear[ndidx].remove( groupidx);}
	/// \brief Evaluate, if there is space left for adding a new relation
	bool hasSpace( const std::size_t& ndidx) const
		{return m_nodear[ndidx].size < m_maxnodesize;}
	/// \brief Evaluate, how much space is left for adding new relations
	unsigned int sizeSpace( const std::size_t& ndidx) const
		{return m_maxnodesize - m_nodear[ndidx].size;}
	/// \brief Evaluate, how many relations exist for a node
	unsigned int nofElements( const std::size_t& ndidx) const
		{return m_nodear[ndidx].size;}

	class const_node_iterator
	{
	public:
		explicit const_node_iterator( const ConceptIndex* ref_)	:ref(ref_){}
		const_node_iterator( const const_node_iterator& o)	:ref(o.ref){}

		const_node_iterator& operator++()			{++ref; return *this;}
		const_node_iterator operator++(int)			{const_node_iterator rt(ref); ++ref; return rt;}

		bool operator==( const const_node_iterator& o) const	{return ref == o.ref;}
		bool operator!=( const const_node_iterator& o) const	{return ref != o.ref;}
		bool operator<( const const_node_iterator& o) const	{return ref < o.ref;}
		bool operator<=( const const_node_iterator& o) const	{return ref <= o.ref;}
		bool operator>( const const_node_iterator& o) const	{return ref > o.ref;}
		bool operator>=( const const_node_iterator& o) const	{return ref >= o.ref;}

		const ConceptIndex& operator*() const			{return *ref;}

	private:
		ConceptIndex const* ref;
	};

	const_node_iterator node_begin( std::size_t ndidx) const	{const Node& nd = m_nodear[ ndidx]; return const_node_iterator( nd.groupidx);}
	const_node_iterator node_end( std::size_t ndidx) const		{const Node& nd = m_nodear[ ndidx]; return const_node_iterator( nd.groupidx + nd.size);}

private:
	void init();

	struct Node
	{
		ConceptIndex size;
		ConceptIndex* groupidx;

		void init( ConceptIndex* groupidx_)
		{
			size = 0;
			groupidx = groupidx_;
		}

		bool insert( const ConceptIndex& gix, std::size_t maxnodesize);
		bool remove( const ConceptIndex& gix);
		bool contains( const ConceptIndex& gidx) const;
		void check( ConceptIndex maxnodesize) const;
	};
	Node* m_nodear;
	ConceptIndex* m_refs;
	std::size_t m_nodearsize;
	ConceptIndex m_maxnodesize;
};

}//namespace
#endif
