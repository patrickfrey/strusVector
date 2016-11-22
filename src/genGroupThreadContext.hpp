/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Multithreading context for the genetic algorithms
#ifndef _STRUS_VECTOR_SPACE_MODEL_GEN_GROUP_THREAD_CONTEXT_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_GEN_GROUP_THREAD_CONTEXT_HPP_INCLUDED
#include "simGroupMap.hpp"
#include "simRelationReader.hpp"
#include "genGroupContext.hpp"
#include <vector>

namespace strus {

typedef void (*GenGroupProcedure)( 
		const GenGroupParameter* parameter,
		SimGroupIdAllocator* groupIdAllocator,
		GenGroupContext* genGroupContext,
		const SimRelationReader* simrelreader,
		std::size_t startidx, std::size_t endidx);

class GenGroupThreadContext
{
public:
	GenGroupThreadContext(
			GlobalCountAllocator* glbcnt_,
			GenGroupContext* groupctx_,
			const SimRelationReader* simrelreader_,
			const GenGroupParameter* parameter_,
			unsigned int nofThreads_);

	void run( GenGroupProcedure proc, std::size_t startidx, std::size_t endidx);

private:
	GlobalCountAllocator* m_glbcnt;
	unsigned int m_nofThreads;
	GenGroupContext* m_groupctx;
	const SimRelationReader* m_simrelreader;
	std::vector<SimGroupIdAllocator> m_allocators;
	const GenGroupParameter* m_parameter;
};

}//namespace
#endif

