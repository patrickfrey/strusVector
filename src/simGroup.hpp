/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for storing similarity group representants (individuals in the genetic algorithm for breeding similarity group representants)
#ifndef _STRUS_VECTOR_SPACE_MODEL_SIMILARITY_GROUP_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SIMILARITY_GROUP_HPP_INCLUDED
#include "simHash.hpp"
#include "strus/base/stdint.h"
#include <vector>
#include <set>

namespace strus {

typedef uint32_t SampleIndex;
typedef uint32_t FeatureIndex;

/// \brief Structure for storing a representant of a similarity group
class SimGroup
{
public:
	SimGroup( const SimHashCollection& samplear, std::size_t m1, std::size_t m2, const FeatureIndex& id_, SimHashAllocator& allocator);
	SimGroup( const SimHash& gencode_, const FeatureIndex& id_)
		:m_id(id_),m_gencode(gencode_),m_age(0),m_members(),m_nofmembers(0){}
	SimGroup( const SimGroup& o)
		:m_id(o.m_id),m_gencode(o.m_gencode),m_age(o.m_age),m_members(o.m_members),m_nofmembers(o.m_nofmembers){}
	SimGroup( const SimGroup& o, const FeatureIndex& id_)
		:m_id(id_),m_gencode(o.m_gencode),m_age(o.m_age),m_members(o.m_members),m_nofmembers(o.m_nofmembers){}
	SimGroup( const SimGroup& o, SimHashAllocator& allocator)
		:m_id(o.m_id),m_gencode(o.m_gencode, allocator),m_age(o.m_age),m_members(o.m_members),m_nofmembers(o.m_nofmembers){}
	SimGroup( const SimGroup& o, const FeatureIndex& id_, SimHashAllocator& allocator)
		:m_id(id_),m_gencode(o.m_gencode, allocator),m_age(o.m_age),m_members(o.m_members),m_nofmembers(o.m_nofmembers){}

	const FeatureIndex& id() const					{return m_id;}
	const SimHash& gencode() const					{return m_gencode;}
	unsigned int age() const					{return m_age;}

	typedef std::set<SampleIndex>::const_iterator const_iterator;

	const_iterator begin() const					{return m_members.begin();}
	const_iterator end() const					{return m_members.end();}

	/// \brief Change the genetic code of this individual to a new value
	void setGencode( const SimHash& gc);
	/// \brief Add a new member, if not member yet
	bool addMember( const SampleIndex& idx);
	/// \brief Remove a member
	bool removeMember( const SampleIndex& idx);
	/// \brief Remove a member reference as iterator
	const_iterator removeMemberItr( const_iterator itr);
	/// \brief Check for a member
	bool isMember( const SampleIndex& idx) const;
	/// \brief Get the number of members in the group
	std::size_t size() const					{return m_nofmembers;}

	/// \brief Get the number of members that differ (edit distance)
	unsigned int diffMembers( const SimGroup& o, unsigned int maxdiff) const;

	/// \brief Calculate the fitness of this individual
	/// \param[in] samplear global array of samples the reference system of this individual is based on
	double fitness( const SimHashCollection& samplear) const;

	/// \brief Do a mutation step
	/// \param[in] samplear global array of samples the reference system of this individual is based on
	void mutate( const SimHashCollection& samplear,
			unsigned int descendants,
			unsigned int maxNofMutations,
			unsigned int maxNofVotes,
			SimHashAllocator& allocator);

	/// \brief Evaluate the kernel = the set of elements that are the same for all samples
	/// \param[in] samplear global array of samples the reference system of this individual is based on
	SimHash kernel( const SimHashCollection& samplear, SimHashAllocator& allocator) const;

	SimGroup& operator=( const SimGroup& o)
	{
		m_id = o.m_id;
		m_gencode = o.m_gencode;
		m_age = o.m_age;
		m_members = o.m_members;
		m_nofmembers = o.m_nofmembers;
		return *this;
	}
	void freeGenCode( SimHashAllocator& allocator)
	{
		allocator.freeVector( m_gencode);
	}

private:
	/// \brief Evaluate the fitness of a proposed genom change
	/// \param[in] samplear global array of samples the reference system of this individual is based on
	double fitness( const SimHashCollection& samplear, const SimHash& candidate) const;
	/// \brief Calculate a mutation
	/// \param[in] samplear global array of samples the reference system of this individual is based on
	/// \param[in] maxNofMutations maximum number of bit mutations to do
	/// \param[in] maxNofVotes number of elements used for a vote for a mutation direction
	SimHash mutation( const SimHashCollection& samplear, unsigned int maxNofMutations, unsigned int maxNofVotes, SimHashAllocator& allocator) const;
	/// \brief Calculate an initial individual (kernel + some random values)
	/// \param[in] samplear global array of samples the reference system of this individual is based on
	SimHash inithash( const SimHashCollection& samplear, SimHashAllocator& allocator) const;
	/// \brief Vote for a mutation
	/// \param[in] mutidx index of gencode element to set
	/// \param[in] nofqueries number of elements to query for the vote
	bool mutation_vote( const SimHashCollection& samplear, unsigned int mutidx, unsigned int nofqueries) const;

private:
	FeatureIndex m_id;			///< feature identifier given to the group
	SimHash m_gencode;			///< genetic code of the group
	unsigned int m_age;			///< virtual value for age of the genom
	std::set<SampleIndex> m_members;	///< members of the group
	std::size_t m_nofmembers;
};

}//namespace
#endif
