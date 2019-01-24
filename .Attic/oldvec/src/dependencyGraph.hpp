/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for thread safe operations on representants of similarity classes with help of genetic algorithms
#ifndef _STRUS_VECTOR_DEPENDENCY_GRAPH_HPP_INCLUDED
#define _STRUS_VECTOR_DEPENDENCY_GRAPH_HPP_INCLUDED
#include <utility>
#include <set>
#include "simGroup.hpp"

namespace strus {

class SampleSimGroupMap;
class SimGroupMap;

/// \brief Dependency from first to second (second contains all elements of first)
typedef std::pair<ConceptIndex,ConceptIndex> Dependency;
typedef std::set<Dependency> DependencyGraph;

/// \brief Build a directed graph of dependencies of groups derived from the maps of samples and groups.
/// \note A group is dependent on another if every of its member is also member of the other group and it has a sufficient fraction of members of the group it is a subset of (isaf parameter)
DependencyGraph buildGroupDependencyGraph( std::size_t nofSamples, const SampleSimGroupMap& sampleSimGroupMap, const SimGroupMap& groupMap, float isaf);

/// \brief Eliminate circular dependencies from a directed graph represented as set
void eliminateCircularReferences( DependencyGraph& graph, const SimGroupMap& groupMap);

}//namespace
#endif


