/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Queue for sample to group assignments
#ifndef _STRUS_VECTOR_SAMPLE_CONCEPT_ASSIGNMENT_QUEUE_HPP_INCLUDED
#define _STRUS_VECTOR_SAMPLE_CONCEPT_ASSIGNMENT_QUEUE_HPP_INCLUDED
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

	bool operator < (const SampleSimGroupAssignment& o) const
	{
		return (conceptIndex == o.conceptIndex) ? (sampleIndex < o.sampleIndex) : (conceptIndex < o.conceptIndex);
	}
};

class SampleSimGroupAssignmentQueue
{
public:
	SampleSimGroupAssignmentQueue(){}

	void push( const SampleIndex& sampleIndex, const ConceptIndex& conceptIndex);

	std::vector<SampleSimGroupAssignment> fetchAll();

private:
	std::vector<SampleSimGroupAssignment> m_ar;
	utils::Mutex m_mutex;
};

}//namespace
#endif


