/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for thread safe operations on representants of similarity classes with help of genetic algorithms
#ifndef _STRUS_VECTOR_GEN_GROUP_CONTEXT_HPP_INCLUDED
#define _STRUS_VECTOR_GEN_GROUP_CONTEXT_HPP_INCLUDED
#include "logger.hpp"
#include "simHash.hpp"
#include "simGroup.hpp"
#include "simGroupMap.hpp"
#include "simRelationMap.hpp"
#include "simRelationReader.hpp"
#include "sampleSimGroupMap.hpp"
#include "sampleSimGroupAssignmentQueue.hpp"
#include "sharedArrayMutex.hpp"
#include "indexListMap.hpp"
#include "strus/base/thread.hpp"
#include <vector>
#include <list>
#include <set>
#include <map>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

struct GenGroupParameter
{
	unsigned int simdist;
	unsigned int eqdist;
	unsigned int descendants;
	unsigned int mutations;
	unsigned int votes;
	unsigned int maxage;
	unsigned int greediness;
	float isaf;
	float baff;
	float fdf;
	float eqdiff;
	bool with_singletons;

	GenGroupParameter()
		:simdist(0),eqdist(0)
		,descendants(0),mutations(0),votes(0),maxage(0),greediness(0)
		,isaf(0.6),baff(0.1),fdf(0.1),eqdiff(0.15),with_singletons(false){}
};

class GenGroupContext
{
public:
	GenGroupContext( GlobalCountAllocator* cnt_, const std::vector<SimHash>& samplear_, std::size_t maxNofGroups_, unsigned int assignments_, unsigned int nofThreads_, const char* logfile_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_)
		,m_logout( logfile_)
		,m_cntalloc(cnt_)
		,m_samplear(&samplear_)
		,m_groupMap(maxNofGroups_)
		,m_sampleSimGroupMap(samplear_.size(), assignments_)
		,m_groupAssignQueue()
		,m_error(){}

	std::vector<ConceptIndex> getNeighbourGroups( const SimGroup& group, unsigned short nbdist, unsigned int maxnofresults);

	void removeGroup(
			SimGroupIdAllocator& localAllocator, const ConceptIndex& group_id);
	bool tryAddGroupMember(
			const ConceptIndex& group_id, const SampleIndex& newmember,
			unsigned int descendants, unsigned int mutations, unsigned int votes,
			unsigned int maxage, double fdf);

	std::string groupMembersString( const ConceptIndex& group_id);
	void checkSimGroupStructures();

	///\brief Eval the group closest to a sample that is closer than a min dist
	ConceptIndex getSampleClosestSimGroup(
			const SampleIndex& main_sampleidx, const SampleIndex& search_sampleidx, 
			unsigned short min_dist);

	/// \brief Find the closest sample to a given sample that is not yet in a group with this sample and can still be assigned to a new group
	bool findClosestFreeSample(
			SimRelationMap::Element& res,
			const SampleIndex& sampleidx,
			const SimRelationReader& simrelreader,
			unsigned int simdist);

	/// \brief Try to make the assignments referenced by thread id of new elements to groups
	void tryGroupAssignments(
			std::vector<SampleSimGroupAssignment>::const_iterator start_itr,
			std::vector<SampleSimGroupAssignment>::const_iterator end_itr,
			const GenGroupParameter& parameter);

	/// \brief Try to group a free feature with its closest free neighbour or attach it to the closest group
	bool greedyChaseFreeFeatures(
			SimGroupIdAllocator& groupIdAllocator,
			const SampleIndex& sidx,
			const SimRelationReader& simrelreader,
			const GenGroupParameter& parameter);

	/// \brief Make elements interchange between neighbour groups with unification of too similar groups:
	void greedyNeighbourGroupInterchange(
			SimGroupIdAllocator& groupIdAllocator,
			const ConceptIndex& gi,
			const GenGroupParameter& parameter,
			unsigned int& interchangecnt);

	/// \brief Remove unfitter of element pairs in eqdist that share most of the elements
	bool improveGroup(
			SimGroupIdAllocator& groupIdAllocator,
			const ConceptIndex& group_id,
			const GenGroupParameter& parameter);

	/// \brief Remove unfitter of element pairs in eqdist that share most of the elements
	bool similarNeighbourGroupElimination(
			SimGroupIdAllocator& groupIdAllocator,
			const ConceptIndex& group_id,
			const GenGroupParameter& parameter);

	/// \brief Eliminate groups with high age and a low fitness factor that are occupying capacity needed
	bool unfittestGroupElimination(
			SimGroupIdAllocator& groupIdAllocator,
			const ConceptIndex& group_id,
			const GenGroupParameter& parameter);

	/// \brief Garbage collection of allocated and leaked sim group idenitifiers
	/// \param[out] nofGroups number of active groups counted
	void garbageCollectSimGroupIds(
			ConceptIndex& nofGroups);

	/// \brief Remove groups that are dependent of others
	void eliminateRedundantGroups( const GenGroupParameter& parameter);

	/// \brief Collect all results
	void collectResults(
			std::vector<SimHash>& results,
			std::vector<SampleIndex>& singletons,
			SampleConceptIndexMap& sampleConceptIndexMap,
			ConceptSampleIndexMap& conceptSampleIndexMap,
			const GenGroupParameter& parameter);

	/// \brief Fetch all assignment instructions collected in the queue and clear the queue
	std::vector<SampleSimGroupAssignment> fetchGroupAssignments();

	Logger& logout()
	{
		return m_logout;
	}
	void reportError( const std::string& msg)
	{
		strus::scoped_lock lock( m_errmutex);
		if (m_logout) m_logout << msg;
		m_error.append( msg);
	}
	const char* lastError() const
	{
		return (m_error.empty()) ? 0:m_error.c_str();
	}
	std::string fetchError()
	{
		std::string rt = m_error;
		m_error.clear();
		return rt;
	}
	const std::vector<SimHash>& samplear() const
	{
		return *m_samplear;
	}

private:
	GenGroupContext( const GenGroupContext&){}		//< non copyable
	void operator=( const GenGroupContext&){}		//< non copyable

private:
	ErrorBufferInterface* m_errorhnd;			//< buffer for errors in thread context
	Logger m_logout;					//< logger for monitoring progress
	GlobalCountAllocator* m_cntalloc;			//< global allocator of group indices
	const std::vector<SimHash>* m_samplear;			//< array of features (samples)
	SimGroupMap m_groupMap;					//< map of similarity groups
	SharedSampleSimGroupMap m_sampleSimGroupMap;		//< map of sample idx to group idx
	SampleSimGroupAssignmentQueue m_groupAssignQueue;	//< queue for cross assignements to groups
	strus::mutex m_errmutex;
	std::string m_error;
};

}//namespace
#endif

