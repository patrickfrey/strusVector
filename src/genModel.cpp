/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for breeding good representants of similarity classes with help of genetic algorithms
#include "genModel.hpp"
#include "sampleSimGroupMap.hpp"
#include "random.hpp"
#include "internationalization.hpp"
#include <ctime>
#include <cmath>
#include <iostream>
#include <sstream>
#include <limits>
#include <algorithm>

using namespace strus;
#undef STRUS_LOWLEVEL_DEBUG

static Random g_random;

class GroupIdAllocator
{
public:
	GroupIdAllocator()
		:m_cnt(0){}
	GroupIdAllocator( const GroupIdAllocator& o)
		:m_cnt(o.m_cnt),m_freeList(o.m_freeList){}

	FeatureIndex alloc()
	{
		FeatureIndex rt;
		if (!m_freeList.empty())
		{
			rt = m_freeList.back();
			m_freeList.pop_back();
		}
		else
		{
			rt = ++m_cnt;
		}
		return rt;
	}

	void free( const FeatureIndex& idx)
	{
		m_freeList.push_back( idx);
	}

	unsigned int nofGroupsAllocated() const
	{
		return m_cnt - m_freeList.size();
	}

	unsigned int nofGroupIdsAllocated() const
	{
		return m_cnt;
	}

private:
	FeatureIndex m_cnt;
	std::vector<FeatureIndex> m_freeList;
};

SimRelationMap GenModel::getSimRelationMap( const std::vector<SimHash>& samplear, const char* logfile) const
{
	std::ostream* logout = 0;
	std::ofstream logfilestream;
	if (logfile)
	{
		if (logfile[0] != '-' || logfile[1])
		{
			try
			{
				logfilestream.open( logfile, std::ofstream::out);
				logout = &logfilestream;
			}
			catch (const std::exception& err)
			{
				throw strus::runtime_error(_TXT("failed to open logfile '%s': %s"), logfile, err.what());
			}
		}
		else
		{
			logout = &std::cerr;
		}
	}
	SimRelationMap rt;
	if (logout) (*logout) << _TXT("calculate similarity relation map") << std::endl << std::endl;
	std::vector<SimHash>::const_iterator si = samplear.begin(), se = samplear.end();
	for (SampleIndex sidx=0; si != se; ++si,++sidx)
	{
		if (logout && sidx % 10000 == 0) (*logout) << _TXT("\rprocessed lines: ") << sidx << ", " << _TXT("number of similarities: ") << rt.nofRelationsDetected() << std::endl;
		std::vector<SimRelationMap::Element> row;

		std::vector<SimHash>::const_iterator pi = samplear.begin();
		for (SampleIndex pidx=0; pi != si; ++pi,++pidx)
		{
			if (pidx != sidx && si->near( *pi, m_simdist))
			{
				unsigned short dist = si->dist( *pi);
#ifdef STRUS_LOWLEVEL_DEBUG
				std::cerr << "declare similarity " << sidx << " ~ " << pidx << " by " << dist << std::endl;
#endif
				row.push_back( SimRelationMap::Element( pidx, dist));
			}
		}
		rt.addRow( sidx, row);
	}
	if (logout) (*logout) << _TXT("\rnumber of samples: ") << samplear.size() << ", " << _TXT("number of similarities: ") << rt.nofRelationsDetected() << std::endl;
	return rt.mirror();
}

typedef std::list<SimGroup> GroupInstanceList;
typedef std::map<FeatureIndex,GroupInstanceList::iterator> GroupInstanceMap;

#ifdef STRUS_LOWLEVEL_DEBUG
static std::string groupMembersString(
		const FeatureIndex& group_id,
		const GroupInstanceList& groupInstanceList,
		const GroupInstanceMap& groupInstanceMap)
{
	std::ostringstream rt;
	GroupInstanceMap::const_iterator group_slot = groupInstanceMap.find( group_id);
	if (group_slot == groupInstanceMap.end())
	{
		throw strus::runtime_error(_TXT("illegal reference in group map (%s): %u"), "tryLeaveUnfitestGroup", group_id);
	}
	GroupInstanceList::const_iterator group_inst = group_slot->second;
	SimGroup::const_iterator mi = group_inst->begin(), me = group_inst->end();
	for (unsigned int midx=0; mi != me; ++mi,++midx)
	{
		if (midx) rt << ", ";
		rt << *mi;
	}
	return rt.str();
}
#endif

static void removeGroup(
		const FeatureIndex& group_id,
		GroupIdAllocator& groupIdAllocator,
		GroupInstanceList& groupInstanceList,
		GroupInstanceMap& groupInstanceMap,
		SampleSimGroupMap& sampleSimGroupMap)
{
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "remove group " << group_id << " {" << groupMembersString( group_id, groupInstanceList, groupInstanceMap) << "}" << std::endl;
#endif
	GroupInstanceMap::iterator group_slot = groupInstanceMap.find( group_id);
	if (group_slot == groupInstanceMap.end()) throw strus::runtime_error(_TXT("illegal reference in group map (%s): %u"), "removeGroup", group_id);
	GroupInstanceList::iterator group_iter = group_slot->second;
	if (group_iter->id() != group_id) throw strus::runtime_error(_TXT("illegal reference in group list"));
	groupInstanceMap.erase( group_slot);

	SimGroup::const_iterator
		mi = group_iter->begin(), me = group_iter->end();
	for (; mi != me; ++mi)
	{
		sampleSimGroupMap.remove( *mi, group_id);
	}
	groupIdAllocator.free( group_id);
	groupInstanceList.erase( group_iter);
}

static unsigned int age_mutations( const SimGroup& group, unsigned int maxage, unsigned int conf_mutations)
{
	return (conf_mutations * (maxage - std::min( maxage, group.age()))) / maxage;
}

static unsigned int age_mutation_votes( const SimGroup& group, unsigned int maxage, unsigned int conf_votes)
{
	return conf_votes * (std::min( group.age(), maxage) / maxage) + 1;
}

static bool tryAddGroupMember( const FeatureIndex& group_id, SampleIndex newmember,
				GroupInstanceMap& groupInstanceMap,
				SampleSimGroupMap& sampleSimGroupMap, const std::vector<SimHash>& samplear,
				unsigned int descendants, unsigned int mutations, unsigned int votes,
				unsigned int maxage)
{
	GroupInstanceMap::iterator group_slot = groupInstanceMap.find( group_id);
	if (group_slot == groupInstanceMap.end()) throw strus::runtime_error(_TXT("illegal reference in group map (%s): %u"), "tryAddGroupMember", group_id);
	GroupInstanceList::iterator group_inst = group_slot->second;

	SimGroup newgroup( *group_inst);
	newgroup.addMember( newmember);
	newgroup.mutate( samplear, descendants, age_mutations( newgroup, maxage, mutations), age_mutation_votes( newgroup, maxage, votes));
	if (newgroup.fitness( samplear) >= group_inst->fitness( samplear))
	{
		*group_inst = newgroup;
		return sampleSimGroupMap.insert( newmember, group_inst->id());
	}
	else
	{
		return false;
	}
}

///\brief Eval the group closest to a sample that is closer than a min dist
static FeatureIndex getSampleClosestSimGroup(
				SampleIndex main_sampleidx, SampleIndex search_sampleidx, 
				unsigned short min_dist,
				const GroupInstanceList& groupInstanceList,
				const GroupInstanceMap& groupInstanceMap,
				const SampleSimGroupMap& sampleSimGroupMap,
				const std::vector<SimHash>& samplear)
{
	FeatureIndex rt = 0;
	SampleSimGroupMap::const_node_iterator gi = sampleSimGroupMap.node_begin( main_sampleidx), ge = sampleSimGroupMap.node_end( main_sampleidx);
	for (; gi != ge; ++gi)
	{
		GroupInstanceMap::const_iterator group_slot = groupInstanceMap.find( *gi);
		if (group_slot == groupInstanceMap.end())
		{
			throw strus::runtime_error(_TXT("illegal reference in group map (%s): %u"), "getSampleClosestSimGroup", *gi);
		}
		GroupInstanceList::const_iterator group_inst = group_slot->second;

		// The new element is closer to a group already existing, then add it there
		if (samplear[ search_sampleidx].near( group_inst->gencode(), min_dist))
		{
			rt = group_inst->id();
			min_dist = samplear[ search_sampleidx].dist( group_inst->gencode());
			break;
		}
	}
	return rt;
}

///\brief Try to find a group with a high age with a low fitness factor and leave it
static bool tryLeaveUnfitestGroup(
		SampleIndex sampleidx,
		unsigned int maxage,
		GroupIdAllocator& groupIdAllocator,
		GroupInstanceList& groupInstanceList,
		GroupInstanceMap& groupInstanceMap,
		SampleSimGroupMap& sampleSimGroupMap,
		const std::vector<SimHash>& samplear)
{
	double minFitness = std::numeric_limits<double>::max();
	double maxFitness = 0.0;
	GroupInstanceList::iterator unfitestGroup = groupInstanceList.end();

	SampleSimGroupMap::const_node_iterator gi = sampleSimGroupMap.node_begin( sampleidx), ge = sampleSimGroupMap.node_end( sampleidx);
	for (; gi != ge; ++gi)
	{
		GroupInstanceMap::iterator group_slot = groupInstanceMap.find( *gi);
		if (group_slot == groupInstanceMap.end())
		{
			throw strus::runtime_error(_TXT("illegal reference in group map (%s): %u"), "tryLeaveUnfitestGroup", *gi);
		}
		GroupInstanceList::iterator group_inst = group_slot->second;
		if (group_inst->id() != *gi)
		{
			throw strus::runtime_error(_TXT("illegal reference in group map (%s): %u"), "tryLeaveUnfitestGroup", *gi);
		}
		double fitness = group_inst->fitness(samplear);
		if (fitness > maxFitness)
		{
			maxFitness = fitness;
		}
		if (group_inst->age() >= ((maxage * STRUS_VECTOR_MAXAGE_MATURE_PERCENTAGE) / 100) && fitness < minFitness)
		{
			minFitness = fitness;
			unfitestGroup = group_inst;
		}
	}
	if (unfitestGroup != groupInstanceList.end() && minFitness < maxFitness * STRUS_VECTOR_BAD_FITNESS_FRACTION)
	{
		FeatureIndex group_id = unfitestGroup->id();
		unfitestGroup->removeMember( sampleidx);
		sampleSimGroupMap.remove( sampleidx, group_id);
		if (unfitestGroup->size() < 2)
		{
			removeGroup( group_id, groupIdAllocator, groupInstanceList, groupInstanceMap, sampleSimGroupMap);
		}
		return true;
	}
	return false;
}

/// \brief Find the closest sample to a given sample that is not yet in a group with this sample and can still be assigned to a new group
static bool findClosestFreeSample( SimRelationMap::Element& res, SampleIndex sampleidx, const SampleSimGroupMap& sampleSimGroupMap, const SimRelationMap& simrelmap)
{
	SimRelationMap::Row row = simrelmap.row( sampleidx);
	SimRelationMap::Row::const_iterator ri = row.begin(), re = row.end();
	for (; ri != re; ++ri)
	{
		if (sampleSimGroupMap.hasSpace( ri->index) && !sampleSimGroupMap.shares( sampleidx, ri->index))
		{
			res = *ri;
			return true;
		}
	}
	return false;
}


typedef std::pair<unsigned int,unsigned int> Dependency;
typedef std::set<Dependency> DependencyGraph;

/// \brief Build a directed graph of dependencies of groups derived from the map of samples to groups.
/// \note A group is dependent on another if every of its member is also member of the other group
static DependencyGraph buildGroupDependencyGraph( std::size_t nofSamples, std::size_t nofGroups, const SampleSimGroupMap& sampleSimGroupMap)
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

static DependencyGraph reduceGroupDependencyGraphToIsa( DependencyGraph& graph, const GroupInstanceMap& groupInstanceMap, float isaf)
{
	DependencyGraph rt;
	std::set<Dependency>::iterator hi = graph.begin(), he = graph.end();
	for (; hi != he; ++hi)
	{
		GroupInstanceMap::const_iterator gi1 = groupInstanceMap.find( hi->first);
		GroupInstanceMap::const_iterator gi2 = groupInstanceMap.find( hi->second);
		if (gi1 == groupInstanceMap.end())
		{
			throw strus::runtime_error(_TXT("missing entry in sim group map (reduce dependencyGraph to isa): %u"), hi->first);
		}
		if (gi2 == groupInstanceMap.end())
		{
			throw strus::runtime_error(_TXT("missing entry in sim group map (reduce dependencyGraph to isa): %u"), hi->second);
		}
		if ((float)gi1->second->size() > (float)gi2->second->size() * isaf)
		{
			rt.insert( *hi);
		}
	}
	return rt;
}

/// \brief Eliminate circular dependencies from a directed graph represented as set
static void eliminateCircularReferences( DependencyGraph& graph)
{
	std::set<Dependency>::iterator hi = graph.begin(), he = graph.end();
	while (hi != he)
	{
		std::set<unsigned int> visited;		// set of visited for not processing nodes more than once
		std::vector<unsigned int> queue;	// queue with candidates to process
		std::size_t qidx = 0;			// iterator in the candidate queue
		unsigned int gid = hi->first;		// current node visited
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

#ifdef STRUS_LOWLEVEL_DEBUG
static void checkSimGroupStructures(
				const GroupInstanceList& groupInstanceList,
				const GroupInstanceMap& groupInstanceMap,
				const SampleSimGroupMap& sampleSimGroupMap,
				std::size_t nofSamples)
{
	std::cerr << "check structures ..." << std::endl;
	sampleSimGroupMap.check();
	std::ostringstream errbuf;
	bool haserr = false;

	GroupInstanceList::const_iterator gi = groupInstanceList.begin(), ge = groupInstanceList.end();
	for (; gi != ge; ++gi)
	{
		std::cerr << "group " << gi->id() << " members = ";
		SimGroup::const_iterator mi = gi->begin(), me = gi->end();
		for (unsigned int midx=0; mi != me; ++mi,++midx)
		{
			if (midx) std::cerr << ", ";
			std::cerr << *mi;
			if (!sampleSimGroupMap.contains( *mi, gi->id()))
			{
				errbuf << _TXT("missing element group relation in sampleSimGroupMap: ") << *mi << " IN " << gi->id() << std::endl;
				haserr = true;
			}
		}
		std::cerr << std::endl;
		gi->check();

		GroupInstanceMap::const_iterator gi_slot = groupInstanceMap.find( gi->id());
		if (gi_slot == groupInstanceMap.end())
		{
			errbuf << _TXT("missing entry in sim group map (check structures): ") << gi->id() << std::endl;
			haserr = true;
		}
	}
	SampleIndex si=0, se=nofSamples;
	for (; si != se; ++si)
	{
		SampleSimGroupMap::const_node_iterator ni = sampleSimGroupMap.node_begin( si), ne = sampleSimGroupMap.node_end( si);
		unsigned int nidx=0;
		if (ni != ne) std::cerr << "sample " << si << " groups = ";
		for (; ni != ne; ++ni,++nidx)
		{
			if (nidx) std::cerr << ", ";
			std::cerr << *ni;

			GroupInstanceMap::const_iterator gi_slot = groupInstanceMap.find( *ni);
			if (gi_slot == groupInstanceMap.end())
			{
				errbuf << _TXT("entry not found in group instance map (check structures): ") << *ni << std::endl;
				haserr = true;
			}
			GroupInstanceList::const_iterator gi = gi_slot->second;
			if (!gi->isMember( si))
			{
				errbuf << _TXT("illegal entry in sim group map (check structures): expected to be member of group: ") << si << " -> " << gi->id() << std::endl;
				haserr = true;
			}
		}
		if (nidx) std::cerr << std::endl;
	}
	if (haserr)
	{
		throw std::runtime_error( errbuf.str());
	}
}
#endif

std::vector<SimHash> GenModel::run(
		const std::vector<SimHash>& samplear,
		const SimRelationMap& simrelmap,
		const char* logfile) const
{
	std::ostream* logout = 0;
	std::ofstream logfilestream;
	if (logfile)
	{
		if (logfile[0] != '-' || logfile[1])
		{
			try
			{
				logfilestream.open( logfile, std::ofstream::out);
				logout = &logfilestream;
			}
			catch (const std::exception& err)
			{
				throw strus::runtime_error(_TXT("failed to open logfile '%s': %s"), logfile, err.what());
			}
		}
		else
		{
			logout = &std::cerr;
		}
	}
	GroupIdAllocator groupIdAllocator;		// Allocator of group ids
	GroupInstanceList groupInstanceList;		// list of similarity group representants
	GroupInstanceMap groupInstanceMap;		// map indices to group representant list iterators
	SampleSimGroupMap sampleSimGroupMap( samplear.size(), m_assignments);	// map of sample idx to group idx

	// Do the iterations of creating new individuals
	unsigned int iteration=0;
	for (; iteration != m_iterations; ++iteration)
	{
		if (logout) (*logout) << _TXT("iteration ") << iteration << ":" << std::endl;
#ifdef STRUS_LOWLEVEL_DEBUG
		checkSimGroupStructures( groupInstanceList, groupInstanceMap, sampleSimGroupMap, samplear.size());
#endif
		if (logout) (*logout) << _TXT("create new individuals") << std::endl;
		// Go through all elements and try to create new groups with the closest free neighbours:
		std::vector<SimHash>::const_iterator si = samplear.begin(), se = samplear.end();
		for (SampleIndex sidx=0; si != se; ++si,++sidx)
		{
			if (!sampleSimGroupMap.hasSpace( sidx))
			{
				if (!tryLeaveUnfitestGroup( sidx, m_maxage, groupIdAllocator, groupInstanceList, groupInstanceMap, sampleSimGroupMap, samplear))
				{
					continue;
				}
			}

			// Find the closest neighbour, that is not yet in a group with this sample:
			SimRelationMap::Element neighbour;
			if (findClosestFreeSample( neighbour, sidx, sampleSimGroupMap, simrelmap))
			{
				// Try to find a group the visited sample belongs to that is closer to the
				// found candidate than the visited sample:
				FeatureIndex bestmatch_simgroup = getSampleClosestSimGroup( sidx, neighbour.index, neighbour.simdist, groupInstanceList, groupInstanceMap, sampleSimGroupMap, samplear);
				if (bestmatch_simgroup)
				{
					// ...if we found such a group, we try to add the candidate there instead:
					if (!tryAddGroupMember( bestmatch_simgroup, neighbour.index, groupInstanceMap, sampleSimGroupMap, samplear, m_descendants, m_mutations, m_votes, m_maxage))
					{
						bestmatch_simgroup = 0;
					}
				}
				if (!bestmatch_simgroup)
				{
					// ...if we did not find such a group we found a new one with the two
					// elements as members:
					SimGroup newgroup( samplear, sidx, neighbour.index, groupIdAllocator.alloc());
					newgroup.mutate( samplear, m_descendants, age_mutations( newgroup, m_maxage, m_mutations), age_mutation_votes( newgroup, m_maxage, m_votes));
					sampleSimGroupMap.insert( neighbour.index, newgroup.id());
					sampleSimGroupMap.insert( sidx, newgroup.id());
					groupInstanceList.push_back( newgroup);
					GroupInstanceList::iterator enditr = groupInstanceList.end();
					groupInstanceMap[ newgroup.id()] = --enditr;
				}
			}
		}
		if (logout) (*logout) << _TXT("unify individuals out of ") << groupIdAllocator.nofGroupsAllocated() << std::endl;

		// Go through all groups and try to make elements jump to neighbour groups and try
		// to unify groups:
		GroupInstanceList::iterator gi = groupInstanceList.begin(), ge = groupInstanceList.end();
		for (; gi != ge; ++gi)
		{
			// Build the set of neighbour groups:
			std::set<FeatureIndex> neighbour_groups;
			SimGroup::const_iterator mi = gi->begin(), me = gi->end();
			for (; mi != me; ++mi)
			{
				SampleSimGroupMap::const_node_iterator si = sampleSimGroupMap.node_begin(*mi), se = sampleSimGroupMap.node_end(*mi);
				for (; si != se; ++si)
				{
					if (*si != gi->id())
					{
						neighbour_groups.insert( *si);
					}
				}
			}
			// Go through all neighbour groups that are in a distance closer to eqdist
			// and delete them if the members are all within the group:
			std::set<FeatureIndex>::const_iterator ni = neighbour_groups.begin(), ne = neighbour_groups.end();
			for (; ni != ne; ++ni)
			{
				GroupInstanceMap::iterator sim_gi_slot = groupInstanceMap.find( *ni);
				if (sim_gi_slot == groupInstanceMap.end()) throw strus::runtime_error(_TXT("illegal reference in group map (%s): %u"), "join eqdist groups", *ni);
				GroupInstanceList::iterator sim_gi = sim_gi_slot->second;

				if (sim_gi->gencode().near( gi->gencode(), m_eqdist))
				{
					// Try to swallow members in sim_gi as long as it is in eqdist:
					SimGroup::const_iterator 
						sim_mi = sim_gi->begin(), sim_me = sim_gi->end();
					for (; sim_mi != sim_me; ++sim_mi)
					{
						if (!gi->isMember( *sim_mi))
						{
							if (!sampleSimGroupMap.hasSpace( *sim_mi)) break;
							// Add member of sim_gi to gi:
							gi->addMember( *sim_mi);
							sampleSimGroupMap.insert( *sim_mi, gi->id());
							gi->mutate( samplear, m_descendants, age_mutations( *gi, m_maxage, m_mutations), age_mutation_votes( *gi, m_maxage, m_votes));
							if (!sim_gi->gencode().near( gi->gencode(), m_eqdist))
							{
								break;
							}
						}
					}
					if (sim_mi == sim_me)
					{
						// ... delete the neighbour group sim_gi where all elements 
						// can be added to gi and it is still in eqdist:
						if (sim_gi->fitness( samplear) < gi->fitness( samplear))
						{
							removeGroup( sim_gi->id(), groupIdAllocator, groupInstanceList, groupInstanceMap, sampleSimGroupMap);
						}
						sim_gi = groupInstanceList.end();
					}
				}
				if (sim_gi != groupInstanceList.end() && gi->gencode().near( sim_gi->gencode(), m_simdist))
				{
					// Try add one member of sim_gi to gi that is similar to gi:
					SimGroup::const_iterator mi = sim_gi->begin(), me = sim_gi->end();
					for (; mi != me; ++mi)
					{
						if (gi->gencode().near( samplear[*mi], m_simdist) && !gi->isMember( *mi))
						{
							if (sampleSimGroupMap.hasSpace( *mi))
							{
								if (tryAddGroupMember( gi->id(), *mi, groupInstanceMap, sampleSimGroupMap, samplear, m_descendants, m_mutations, m_votes, m_maxage))
								{
									break;
								}
							}
						}
					}
					// Eliminate similar group that contains most of this group members and that has a smaller fitness:
					unsigned int eqdiff = std::min( gi->size(), sim_gi->size()) / 4;
					if (gi->diffMembers( *sim_gi, eqdiff) < eqdiff)
					{
						if (sim_gi->fitness( samplear) < gi->fitness( samplear))
						{
							removeGroup( sim_gi->id(), groupIdAllocator, groupInstanceList, groupInstanceMap, sampleSimGroupMap);
						}
						sim_gi = groupInstanceList.end();
					}
				}
			}
		}
		if (logout) (*logout) << _TXT("start mutation step") << std::endl;

		// Mutation step for all groups and dropping of elements that got too far away from the
		// representants genom:
		gi = groupInstanceList.begin(), ge = groupInstanceList.end();
		for (; gi != ge; ++gi)
		{
			gi->mutate( samplear, m_descendants, age_mutations( *gi, m_maxage, m_mutations), age_mutation_votes( *gi, m_maxage, m_votes));

			SimGroup::const_iterator mi = gi->begin(), me = gi->end();
			for (std::size_t midx=0; mi != me; ++mi,++midx)
			{
				unsigned int dist = m_raddist + ((m_simdist - m_raddist) * (m_iterations - iteration - 1) / (m_iterations - 1));
				if (!gi->gencode().near( samplear[ *mi], dist))
				{
					// Dropped members that got too far out of the group:
					SampleIndex member = *mi;
					mi = gi->removeMemberItr( mi);
					--mi;
					--midx;
					sampleSimGroupMap.remove( member, gi->id());
					if (gi->size() < 2) break;
				}
			}
			if (gi->size() < 2)
			{
				// Delete group that lost too many members:
				FeatureIndex gid = gi->id();
				++gi;
				removeGroup( gid, groupIdAllocator, groupInstanceList, groupInstanceMap, sampleSimGroupMap);
				--gi;
			}
		}
	}
	if (logout) (*logout) << _TXT("eliminate redundant groups") << std::endl;
	{
		// Build the dependency graph:
		DependencyGraph groupDependencyGraph = buildGroupDependencyGraph( samplear.size(), groupIdAllocator.nofGroupIdsAllocated(), sampleSimGroupMap);
		groupDependencyGraph = reduceGroupDependencyGraphToIsa( groupDependencyGraph, groupInstanceMap, m_isaf);
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << "dependencies before elimination:" << std::endl;
		{
			DependencyGraph::const_iterator di = groupDependencyGraph.begin(), de = groupDependencyGraph.end();
			for (; di != de; ++di)
			{
				std::cerr << "(" << di->first << "," << di->second << ") {" << groupMembersString(di->first,groupInstanceList,groupInstanceMap) << "}" << std::endl;
			}
		}
#endif
		// Eliminate circular references from the graph:
		eliminateCircularReferences( groupDependencyGraph);
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << "dependencies after elimination:" << std::endl;
		{
			DependencyGraph::const_iterator di = groupDependencyGraph.begin(), de = groupDependencyGraph.end();
			for (; di != de; ++di)
			{
				std::cerr << "(" << di->first << "," << di->second << ") {" << groupMembersString(di->first,groupInstanceList,groupInstanceMap) << "}" << std::endl;
			}
		}
#endif
		// Eliminate the groups dependent of others:
		DependencyGraph::const_iterator di = groupDependencyGraph.begin(), de = groupDependencyGraph.end();
		while (di != de)
		{
			unsigned int gid = di->first;
			removeGroup( gid, groupIdAllocator, groupInstanceList, groupInstanceMap, sampleSimGroupMap);
			for (++di; di != de && gid == di->first; ++di){}
		}
	}
	// Build the result:
	if (logout) (*logout) << _TXT("build the result") << std::endl;
	std::vector<SimHash> rt;

	// Count the number of singletons and possibly add them to the result:
	SampleIndex si=0, se=samplear.size();
	unsigned int nofSigletons = 0;
	for (; si != se; ++si)
	{
		if (sampleSimGroupMap.nofElements( si) == 0)
		{
			++nofSigletons;
			if (m_with_singletons)
			{
				rt.push_back( samplear[ si]);
			}
		}
	}
	if (logout) (*logout) << _TXT("number of singletons found: ") << nofSigletons << std::endl;

	// Add the groups to the result:
	GroupInstanceList::const_iterator gi = groupInstanceList.begin(), ge = groupInstanceList.end();
	for (; gi != ge; ++gi)
	{
		rt.push_back( gi->gencode());
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "got " << rt.size() << " categories" << std::endl;
	gi = groupInstanceList.begin(), ge = groupInstanceList.end();
	for (; gi != ge; ++gi)
	{
		std::cerr << "category " << gi->id() << ": ";
		SimGroup::const_iterator mi = gi->begin(), me = gi->end();
		for (unsigned int midx=0; mi != me; ++mi,++midx)
		{
			if (midx) std::cerr << ", ";
			std::cerr << *mi;
		}
		std::cerr << std::endl;
	}
#endif
	if (logout) (*logout) << _TXT("done, got categories ") << rt.size() << std::endl;
	return rt;
}

std::string GenModel::tostring() const
{
	std::ostringstream rt;
	rt << "simdist=" << m_simdist << std::endl
		<< ", raddist=" << m_eqdist << std::endl
		<< ", eqdist=" << m_eqdist << std::endl
		<< ", mutations=" << m_mutations << std::endl
		<< ", votes=" << m_votes << std::endl
		<< ", descendants=" << m_descendants << std::endl
		<< ", maxage=" << m_maxage << std::endl
		<< ", iterations=" << m_iterations << std::endl
		<< ", assignments=" << m_assignments << std::endl
		<< ", isaf=" << m_isaf << std::endl
		<< ", singletons=" << (m_with_singletons?"yes":"no") << std::endl;
	return rt.str();
}


