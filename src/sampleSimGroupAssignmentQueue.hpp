/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Queue for sample to group assignments
#ifndef _STRUS_VECTOR_SPACE_MODEL_SAMPLE_CONCEPT_ASSIGNMENT_QUEUE_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SAMPLE_CONCEPT_ASSIGNMENT_QUEUE_HPP_INCLUDED
#include "simGroup.hpp"
#include "utils.hpp"
#include <vector>

namespace strus {

struct SampleSimGroupAssignment
{
public:
	SampleIndex sampleIndex;
	ConceptIndex conceptIndex;

	SampleSimGroupAssignment( const SampleIndex& sampleIndex_, const ConceptIndex& conceptIndex_)
		:sampleIndex(sampleIndex_),conceptIndex(conceptIndex_){}
	SampleSimGroupAssignment( const SampleSimGroupAssignment& o)
		:sampleIndex(o.sampleIndex),conceptIndex(o.conceptIndex){}
};

class SampleSimGroupAssignmentQueue
{
public:
	SampleSimGroupAssignmentQueue(){}

	void push( const SampleIndex& sampleIndex, const ConceptIndex& conceptIndex)
	{
		utils::ScopedLock lock( m_mutex);
		m_ar.push_back( SampleSimGroupAssignment( sampleIndex, conceptIndex));
	}

	void clear()
	{
		utils::ScopedLock lock( m_mutex);
		m_ar.clear();
	}

	const std::vector<SampleSimGroupAssignment>& get() const	{return m_ar;}

private:
	std::vector<SampleSimGroupAssignment> m_ar;
	utils::Mutex m_mutex;
};

class SampleSimGroupAssignmentDispQueue
{
public:
	explicit SampleSimGroupAssignmentDispQueue( std::size_t arsize_=1);
	~SampleSimGroupAssignmentDispQueue();

	void setChunkSize( std::size_t chunksize_)
	{
		m_chunksize = chunksize_;
	}

	void push( const SampleIndex& sampleIndex, const ConceptIndex& conceptIndex);

	const std::vector<SampleSimGroupAssignment>& get( std::size_t idx) const;

	void clear();

private:
	SampleSimGroupAssignmentQueue** m_ar;
	std::size_t m_arsize;
	std::size_t m_chunksize;
};

}//namespace
#endif


