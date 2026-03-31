# Concrete Mode Unification Plan

Goal: make a file such as quantas/KademliaPeer/KademliaPeerInput.json mean the same thing in abstract and concrete mode without modifying any application directory and without changing abstract-mode code.

Non-goals for the first pass:
- Do not rename or edit any files under quantas/*Peer.
- Do not change quantas/Common/Abstract.
- Do not try to make application-specific end-of-round aggregation fully identical yet.

## Target behavior

The same experiment JSON must drive both runtimes:
- algorithms stays the compilation list.
- experiments[n].topology stays the topology source of truth.
- experiments[n].parameters stays the peer configuration payload.
- experiments[n].topology.initialPeerType stays the peer type name in both runtimes.

Concrete mode should differ only in transport and process model:
- abstract mode: one process, many peers, in-memory channels.
- concrete mode: many processes, one active peer per process, TCP transport.

## Current blockers in the concrete path

1. quantas/Common/Concrete/concreteSimulation.cpp hardcodes AltBitPeerConcrete instead of reading topology.initialPeerType.
2. quantas/Common/Concrete/NetworkInterfaceConcrete.hpp bootstraps from a leader config that does not understand the experiment JSON.
3. Concrete mode builds a complete graph by discovery instead of respecting the experiment topology.
4. Some peer initialization code expects a vector containing all peers, not just the local one.
5. End-of-round metrics are often computed by iterating over every peer instance, which cannot be reproduced exactly without application cooperation.

## Refactor plan

### Phase 1: Shared experiment parsing in concrete mode

Add a concrete-only experiment loader under quantas/Common/Concrete that:
- reads the same input JSON file used by abstract mode;
- selects one experiment by index;
- exposes:
  - algorithms;
  - topology;
  - parameters;
  - distribution;
  - rounds;
  - tests.

Suggested files:
- quantas/Common/Concrete/ConcreteExperiment.hpp
- quantas/Common/Concrete/ConcreteTopologyPlan.hpp

The loader must not depend on quantas/Common/Abstract.

### Phase 2: Rebuild topology logic locally in the concrete runtime

Copy the topology semantics from abstract mode into a concrete-only planner.

Supported topology types must match abstract mode:
- complete
- star
- grid
- torus
- chain
- ring
- unidirectionalRing
- userList

Implementation rule:
- the planner returns an ordered list of public ids and an adjacency list keyed by public id.

Important detail:
- if topology.identifiers == random, the leader must generate the randomized public-id order once and distribute it to every process.
- do not try to infer the same random order independently on each process.

### Phase 3: Separate bootstrap config from experiment config

Concrete mode should use two inputs:
- experiment JSON: the same file as abstract mode;
- bootstrap JSON or CLI arguments: only deployment details such as leader ip, leader port, and optional local port.

Bootstrap data must not redefine:
- total_peers
- topology
- peer type
- parameters

Those values must come from the experiment JSON only.

### Phase 4: Replace full-mesh neighbor discovery with topology assignment

Change the leader protocol in quantas/Common/Concrete/NetworkInterfaceConcrete.hpp:
- keep peer discovery to learn ip and port of each process;
- assign each process a public id;
- send every process:
  - the full public-id to endpoint map;
  - the public-id order used by topology;
  - its own public id.

After bootstrap, each process must compute its own neighbor set from the shared topology plan and call addNeighbor only for those peers.

Current behavior to remove:
- adding every discovered peer as a neighbor.

### Phase 5: Instantiate peers by the same name as abstract mode

Concrete mode must stop relying on names such as AltBitPeerConcrete.

Add a concrete-only factory under quantas/Common/Concrete that maps the abstract peer name to a constructor using NetworkInterfaceConcrete.

Suggested file:
- quantas/Common/Concrete/ConcretePeerFactory.hpp

Example contract:
- input: KademliaPeer
- output: new KademliaPeer(new NetworkInterfaceConcrete())

This keeps the meaning of topology.initialPeerType unchanged.

### Phase 6: Create shadow peers for initParameters

Many peer implementations configure themselves by iterating over the full peer vector during initParameters.
Because application code cannot be changed, each concrete process should create:
- one active local peer with NetworkInterfaceConcrete;
- N-1 shadow peers of the same concrete type with a no-op interface.

The shadow peers only need:
- stable public ids;
- neighbor sets derived from the shared topology plan.

Suggested file:
- quantas/Common/Concrete/NullNetworkInterface.hpp

Then the concrete runtime can call localPeer->initParameters(allPeers, parameters) with the same shape as abstract mode.

This is enough to preserve initialization semantics for:
- committee construction;
- all-peer parameter fanout;
- fault attachment by index;
- DHT id bookkeeping.

### Phase 7: Keep the concrete event loop generic

Rewrite quantas/Common/Concrete/concreteSimulation.cpp so that it:
- parses the shared experiment file;
- chooses the peer type from experiments[n].topology.initialPeerType;
- constructs the active peer via ConcretePeerFactory;
- constructs shadow peers via NullNetworkInterface;
- calls initParameters with the full peer vector;
- runs only the active peer in the local receive/compute loop.

The loop must no longer hardcode AltBit behavior.

### Phase 8: Update the concrete makefile to compile the same algorithms list

The concrete build should mirror the root makefile behavior for algorithms:
- parse the algorithms array from INPUTFILE;
- compile those translation units;
- link them into the concrete executable together with the concrete runtime files.

This keeps the meaning of each application directory unchanged:
- if the experiment uses quantas/KademliaPeer/KademliaPeerInput.json, concrete mode compiles KademliaPeer/KademliaPeer.cpp.

### Phase 9: Preserve message semantics incrementally

Topology and peer selection should be unified first.
After that, move concrete transport closer to abstract distribution semantics.

Priority order:
1. support delay;
2. support dropProbability;
3. support duplicateProbability;
4. support reorderProbability;
5. support queue size and maxMsgsRec.

These belong in quantas/Common/Concrete/NetworkInterfaceConcrete.hpp or a new concrete channel helper.

## Known limitation that cannot be solved generically under the current constraints

Exact end-of-round metrics cannot be guaranteed without changing application code.

Reason:
- many peers compute final metrics by iterating over all live peer objects and reading application-specific local state;
- in concrete mode, only one live peer exists per process;
- shadow peers do not evolve with remote state.

Acceptable first-pass behavior:
- unify experiment meaning, topology, peer selection, and initialization;
- document that final aggregated metrics remain concrete-mode-specific until a generic snapshot or metrics API exists.

## Concrete implementation order

1. Add ConcreteExperiment and ConcreteTopologyPlan helpers.
2. Add ConcretePeerFactory using the existing peer headers.
3. Add NullNetworkInterface for shadow peers.
4. Refactor NetworkInterfaceConcrete bootstrap to assign public ids and stop full-mesh neighbor registration.
5. Refactor concreteSimulation.cpp to use the same experiment file and peer type name as abstract mode.
6. Update concrete/makefile to compile algorithms from INPUTFILE.
7. Validate AltBit, StableDataLink, Kademlia, and ExamplePeer end-to-end in concrete mode.
8. Add transport-level delay/drop/duplicate behavior.

## Validation checklist

- Same INPUTFILE path works in abstract and concrete mode.
- Same topology.initialPeerType value works in abstract and concrete mode.
- Concrete neighbors match the topology generated from the experiment JSON.
- initParameters sees a full peer vector in concrete mode.
- No code under quantas/*Peer is modified.
- No code under quantas/Common/Abstract is modified.