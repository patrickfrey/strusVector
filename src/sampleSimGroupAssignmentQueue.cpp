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

void SampleSimGroupAssignmentQueue::push( const SampleIndex& sampleIndex, const ConceptIndex& conceptIndex)
{
	utils::ScopedLock lock( m_mutex);
	m_ar.push_back( SampleSimGroupAssignment( sampleIndex, conceptIndex));
}

std::vector<SampleSimGroupAssignment> SampleSimGroupAssignmentQueue::fetchAll()
{
	std::vector<SampleSimGroupAssignment> rt;
	{
		utils::ScopedLock lock( m_mutex);
		rt = m_ar;
	}
	m_ar.clear();
	return rt;
}

