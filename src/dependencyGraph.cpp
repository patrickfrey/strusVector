/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for thread safe operations on representants of similarity classes with help of genetic algorithms
#include "dependencyGraph.hpp"
#include "internationalization.hpp"
#include "sampleSimGroupMap.hpp"
#include "simGroupMap.hpp"
#include "genGroupContext.hpp"

using namespace strus;
using namespace strus::utils;

typedef std::pair<ConceptIndex,ConceptIndex> Dependency;
typedef std::set<Dependency> DependencyGraph;

DependencyGraph strus::buildGroupDependencyGraph( std::size_t nofSamples, std::size_t nofGroups, const SampleSimGroupMap& sampleSimGroupMap)
{
	DependencyGraph rt;
	std::vector<unsigned char> independentmark( nofGroups, 0); /*values are 0,1,2*/
	SampleIndex si=0, se=nofSamples;
	for (; si < se; ++si)
	{
		SampleSimGroupMap::const_node_iterator ni = sampleSimGroupMap.node_begin( si), ne = sampleSimGroupMap.node_end( si);
		SampleSimGroupMap::const_node_iterator na = ni;
		for (; ni != ne; ++ni)
		{
			if (independentmark[ *ni-1] == 2)
			{}
			else if (independentmark[ *ni-1] == 0)
			{
				SampleSimGroupMap::const_node_iterator oni = na, one = ne;
				for (; oni != one; ++oni)
				{
					if (*oni != *ni)
					{
						// postulate *ni dependent from *oni
						rt.insert( Dependency( *ni, *oni));
					}
				}
				independentmark[ *ni-1] = 1;
			}
			else if (independentmark[ *ni-1] == 1)
			{
				bool have_found = false;
				std::set<Dependency>::iterator di = rt.upper_bound( Dependency( *ni, 0));
				while (di != rt.end() && di->first == *ni)
				{
					if (sampleSimGroupMap.contains( si, di->second))
					{
						have_found = true;
						++di;
					}
					else
					{
						std::set<Dependency>::iterator di_del = di;
						++di;
						rt.erase( di_del);
					}
				}
				if (!have_found)
				{
					independentmark[ *ni-1] = 2;
					continue;
				}
			}
			else
			{
				throw strus::runtime_error(_TXT("internal: wrong group dependency status: %u [%u]"), independentmark[ *ni-1], *ni);
			}
		}
	}
	return rt;
}

DependencyGraph strus::reduceGroupDependencyGraphToIsa( DependencyGraph& graph, const SimGroupMap& groupMap, float isaf)
{
	DependencyGraph rt;
	std::set<Dependency>::iterator hi = graph.begin(), he = graph.end();
	for (; hi != he; ++hi)
	{
		SimGroupRef gi1 = groupMap.get( hi->first);
		SimGroupRef gi2 = groupMap.get( hi->second);
		if (!gi1.get())
		{
			throw strus::runtime_error(_TXT("missing entry in sim group map (reduce dependencyGraph to isa): %u"), hi->first);
		}
		if (!gi2.get())
		{
			throw strus::runtime_error(_TXT("missing entry in sim group map (reduce dependencyGraph to isa): %u"), hi->second);
		}
		if ((float)gi1->size() > (float)gi2->size() * isaf)
		{
			rt.insert( *hi);
		}
	}
	return rt;
}

void strus::eliminateCircularReferences( DependencyGraph& graph)
{
	std::set<Dependency>::iterator hi = graph.begin(), he = graph.end();
	while (hi != he)
	{
		std::set<ConceptIndex> visited;		// set of visited for not processing nodes more than once
		std::vector<ConceptIndex> queue;	// queue with candidates to process
		std::size_t qidx = 0;			// iterator in the candidate queue
		ConceptIndex gid = hi->first;		// current node visited
		// Iterate through all vertices pointed to by arcs rooted in the current node visited:
		for (; hi != he && gid == hi->first; ++hi)
		{
			// Insert the visited vertex into the queue of nodes to process:
			if (visited.insert( hi->second).second)
			{
				queue.push_back( hi->second);
			}
		}
		// Iterate through the set of reachable vertices and check if they have an arc pointing to
		//   the current node visited. If yes, eliminate this arc to eliminate the circular dependency:
		for (; qidx < queue.size(); ++qidx)
		{
			std::set<Dependency>::iterator
				dep_hi = graph.find( Dependency( queue[qidx], gid));
			if (dep_hi != graph.end())
			{
				// Found a circular reference, eliminate the dependency to the current node:
				if (hi == dep_hi)
				{
					unsigned int gid = hi->first;
					graph.erase( dep_hi);
					hi = graph.upper_bound( Dependency(gid,0));
				}
				else
				{
					graph.erase( dep_hi);
				}
			}
			// Expand the set of vertices directed to from the follow arcs of the processed edge:
			std::set<Dependency>::iterator
				next_hi = graph.upper_bound( Dependency( queue[qidx], 0));
			for (; next_hi != graph.end() && queue[qidx] == next_hi->first; ++next_hi)
			{
				if (visited.insert( next_hi->second).second)
				{
					queue.push_back( hi->second);
				}
			}
		}
	}
}
