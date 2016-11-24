/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Queue for sample to group assignments
#include "sampleSimGroupAssignmentQueue.hpp"
#include "utils.hpp"
#include "cacheLineSize.hpp"
#include "internationalization.hpp"
#include <vector>
#include <limits>
#include <stdexcept>

using namespace strus;

SampleSimGroupAssignmentDispQueue::SampleSimGroupAssignmentDispQueue( std::size_t arsize_)
	:m_ar(0),m_arsize(arsize_),m_chunksize(std::numeric_limits<std::size_t>::max())
{
	m_ar = (SampleSimGroupAssignmentQueue**) std::calloc( m_arsize, sizeof(SampleSimGroupAssignmentQueue*));
	if (!m_ar) throw std::bad_alloc();
	std::size_t ai = 0, ae = m_arsize;
	for (; ai != ae; ++ai)
	{
		m_ar[ai] = (SampleSimGroupAssignmentQueue*) utils::aligned_malloc( sizeof(SampleSimGroupAssignmentQueue), CacheLineSize);
		if (!m_ar[ai]) goto NOMEM;
		try
		{
			new (m_ar[ai]) SampleSimGroupAssignmentQueue();
		}
		catch (const std::bad_alloc&)
		{
			goto NOMEM;
		}
	}
	return;
NOMEM:
	for (; ai != 0; --ai)
	{
		m_ar[ ai-1]->~SampleSimGroupAssignmentQueue();
		utils::aligned_free( m_ar[ ai-1]);
	}
	std::free( m_ar);
	throw std::bad_alloc();
}

SampleSimGroupAssignmentDispQueue::~SampleSimGroupAssignmentDispQueue()
{
	std::size_t ai = 0, ae = m_arsize;
	for (; ai != ae; ++ai)
	{
		m_ar[ ai]->~SampleSimGroupAssignmentQueue();
		utils::aligned_free( m_ar[ ai]);
	}
	std::free( m_ar);
}

void SampleSimGroupAssignmentDispQueue::push( const SampleIndex& sampleIndex, const ConceptIndex& conceptIndex)
{
	std::size_t queueidx = conceptIndex / m_chunksize;
	if (queueidx >= m_arsize) throw strus::runtime_error(_TXT("array bound write in %s"), "SampleSimGroupAssignmentDispQueue");
	m_ar[ queueidx]->push( sampleIndex, conceptIndex);
}

const std::vector<SampleSimGroupAssignment>& SampleSimGroupAssignmentDispQueue::get( std::size_t idx) const
{
	if (idx >= m_arsize) throw strus::runtime_error(_TXT("array bound read in %s"), "SampleSimGroupAssignmentDispQueue");
	return m_ar[idx]->get();
}

void SampleSimGroupAssignmentDispQueue::clear()
{
	std::size_t ai = 0, ae = m_arsize;
	for (; ai != ae; ++ai) m_ar[ai]->clear();
}

