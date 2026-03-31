# Concrete Mode Unification Status

Goal: make a file such as quantas/KademliaPeer/KademliaPeerInput.json mean the same thing in abstract and concrete mode without modifying any application directory and without changing abstract-mode code.

This document is no longer a proposal. It describes the concrete runtime as it exists now, what has been validated, and what remains open.

## Invariants kept

- No file under quantas/*Peer was modified to make concrete mode work.
- No file under quantas/Common/Abstract was modified.
- The same experiment JSON drives both runtimes.
- experiments[n].topology.initialPeerType keeps the same meaning in both runtimes.
- experiments[n].parameters keeps the same meaning in both runtimes.
- experiments[n].topology remains the topology source of truth in both runtimes.

## Runtime model now implemented

Abstract mode and concrete mode differ only in transport and process model.

- abstract mode: one process, many peer objects, in-memory channels.
- concrete mode: one active peer per process, many OS processes, TCP transport, central logger for bootstrap and aggregation.

The concrete runtime no longer hardcodes any application-specific peer type.

## Concrete architecture implemented

### Shared experiment parsing

Implemented in:
- quantas/Common/Concrete/ConcreteExperiment.hpp

Current behavior:
- loads the same INPUTFILE JSON used by abstract mode;
- selects one experiment by index;
- exposes topology, parameters, distribution, rounds, tests, and input file metadata;
- does not depend on quantas/Common/Abstract.

### Topology reconstruction in concrete mode

Implemented in:
- quantas/Common/Concrete/ConcreteTopologyPlan.hpp

Current behavior:
- reconstructs the topology locally from the experiment JSON;
- distributes one shared topology plan to all peers through the logger bootstrap protocol;
- supports the same topology family intended by the abstract configuration path;
- preserves identifier assignment centrally so concrete peers do not invent their own public-id ordering.

### Bootstrap split from experiment config

Implemented in:
- quantas/Common/Concrete/ConcreteBootstrap.hpp

Concrete mode now consumes two inputs:
- experiment JSON: same meaning as abstract mode;
- bootstrap JSON: deployment-only details.

Bootstrap currently contains:
- logger ip;
- logger port;
- aggregate output path;
- detailed output path.

Bootstrap no longer defines:
- peer type;
- topology;
- number of peers;
- parameters.

### Logger-centered bootstrap and aggregation

Implemented in:
- quantas/Common/Concrete/concreteLogger.cpp
- quantas/Common/Concrete/ConcreteLoggerProtocol.hpp
- quantas/Common/Concrete/ConcreteLoggerClient.hpp

The logger is now the central rendezvous for concrete mode.

Current responsibilities:
- receive peer registrations;
- assign public ids;
- distribute the full endpoint map and topology plan;
- collect final peer reports;
- write two output files:
  - a compact aggregate intended to resemble abstract output;
  - a detailed file containing peerReports plus embedded aggregate.

Important transport fixes already implemented:
- read complete socket payloads until EOF instead of assuming one recv/read is enough;
- send full buffers on both peer and logger side;
- retry logger connections from peers;
- use a larger listen backlog in the logger.

### Concrete peer factory and shadow peers

Implemented in:
- quantas/Common/Concrete/ConcretePeerFactory.hpp
- quantas/Common/Concrete/NullNetworkInterface.hpp

Current behavior:
- concrete mode instantiates the same peer type names as abstract mode;
- each process creates one active peer with NetworkInterfaceConcrete;
- each process creates shadow peers with NullNetworkInterface so initParameters still sees a full peer vector.

This preserves initialization semantics used by existing applications for:
- committee construction;
- per-peer parameter fanout;
- fault assignment by index;
- DHT bookkeeping based on peer identity.

### Generic concrete event loop

Implemented in:
- quantas/Common/Concrete/concreteSimulation.cpp

Current behavior:
- parses the shared experiment JSON;
- selects the peer type from experiments[n].topology.initialPeerType;
- constructs the active peer via ConcretePeerFactory;
- constructs shadow peers via NullNetworkInterface;
- calls initParameters with the full peer vector;
- runs receive/compute only on the active peer;
- uses experiment.rounds() instead of a hardcoded limit;
- finds the actual active peer at shutdown before sending the final report.

The last point matters for applications like ExamplePeer where the original local peer object can be replaced by another peer object during execution.

### Concrete transport semantics

Implemented in:
- quantas/Common/Concrete/NetworkInterfaceConcrete.hpp

Current behavior approximates abstract distribution semantics locally for incoming concrete messages:
- delay;
- dropProbability;
- duplicateProbability;
- reorderProbability;
- queue size;
- maxMsgsRec.

Concrete neighbors are derived from the shared topology plan, not from full-mesh discovery.

### Concrete build behavior

Implemented in:
- concrete/makefile

Current behavior:
- parses algorithms from INPUTFILE, like the root build;
- compiles the corresponding application translation units;
- rebuilds the shared concrete runtime objects automatically when INPUTFILE changes;
- exposes per-application helper targets such as release-bitcoin, release-raft, run-kademlia, and similar variants.

Important lesson recorded in code:
- changing only INPUTFILE was previously unsafe because concreteSimulation.o and concreteLogger.o could stay compiled for the previous application family;
- the current build-context stamp fixes that by invalidating the shared concrete runtime when the selected build context changes.

### Aggregate preparation and comparison tooling

Implemented in:
- concrete/aggregate_tools.py
- concrete/makefile

Current behavior:
- can prepare a mono-experiment abstract config with an overridden log file;
- can compare one abstract aggregate against one concrete aggregate;
- can write a JSON comparison report for inspection.

Helper targets:
- make prepare-abstract-config
- make compare-aggregates

## Aggregation behavior now implemented

### Dual concrete outputs

The logger now writes:
- one compact aggregate file;
- one detailed file with per-peer reports and the compact aggregate embedded.

Bootstrap examples live under concrete/config-*-local.json.

### Generic reduction path

Implemented in:
- quantas/Common/Concrete/ConcreteLoggerProtocol.hpp

Current reducer framework supports:
- scalar sum;
- scalar max;
- elementwise sum for numeric arrays;
- resampled median arrays;
- raw collection of values.

### Kademlia-specific short-term reduction

Implemented in:
- quantas/Common/Concrete/ConcreteLoggerProtocol.hpp

Current special handling for Kademlia:
- kademliaAverageHops and kademliaAverageLatency use resampled median arrays with leading-zero trimming;
- kademliaRequestsSatisfied uses resampled median arrays without trimming.

This was needed because Kademlia logs global metrics from every peer, so naive summation was structurally wrong.

## Applications validated in concrete mode

Validated end-to-end with compact and detailed outputs:
- AltBitPeer
- ExamplePeer
- StableDataLinkPeer
- KademliaPeer
- LinearChordPeer
- PBFTPeer
- RaftPeer
- BitcoinPeer
- EthereumPeer

Concrete bootstrap files currently present:
- concrete/config-example-local.json
- concrete/config-kademlia-local.json
- concrete/config-stabledatalink-local.json
- concrete/config-linearchord-local.json
- concrete/config-pbft-local.json
- concrete/config-raft-local.json
- concrete/config-bitcoin-local.json
- concrete/config-ethereum-local.json

## Abstract versus concrete comparison status

Comparable abstract and concrete aggregates were produced for:
- Kademlia
- PBFT
- Raft
- Bitcoin
- Ethereum

Observed patterns:
- top-level shape now generally matches: tests, RunTime, Peak Memory KB;
- some applications still differ significantly in per-metric array lengths or value semantics;
- asynchronous concrete time can produce much denser series than abstract round-based execution;
- some applications still expose per-peer structures in concrete mode where abstract mode logs a single aggregate summary.

Known examples:
- Raft concrete output is much denser than abstract output because the application logs against asynchronous currentRound behavior;
- PBFT concrete output currently has the right metric keys but a much shorter time series shape;
- Bitcoin and Ethereum keep matching metric names, but some fork-location style metrics remain collected per peer rather than merged into one abstract-like structure.

## Known limitations still open

### Exact end-of-round equivalence is not generic yet

This remains the main unresolved problem.

Reason:
- many applications compute metrics by iterating over all live peer objects and reading local state;
- in concrete mode, shadow peers preserve initialization semantics but do not evolve with remote runtime state;
- some applications log once per abstract round, while concrete mode uses asynchronous elapsed-time rounds.

Consequence:
- compact concrete aggregates are often structurally closer to abstract output than before, but still not fully identical in general.

### Reducers are still partly application-specific

The reducer framework exists, but only Kademlia has explicit semantic reduction rules so far.

Open work:
- define reducer rules for PBFT;
- define reducer rules for Raft;
- define reducer rules for Bitcoin;
- define reducer rules for Ethereum;
- decide when to keep per-peer details only in the detailed file and when to compress them into a single abstract-like compact metric.

### Asynchronous time semantics remain a structural gap

Concrete mode still uses RoundManager::asynchronous() and wall-clock-derived currentRound().

This means:
- applications may emit many more samples in concrete mode than in abstract mode;
- even with reducer logic, some compact aggregates will remain denser than their abstract equivalents.

## Short next steps

The highest-value remaining tasks are:
1. add reducer rules for PBFT and Raft so compact concrete outputs better match abstract time-series shape;
2. add reducer rules for Bitcoin and Ethereum for fork-structure metrics;
3. decide whether concrete mode should keep asynchronous time semantics or introduce a stricter round-aligned reporting path for comparison purposes.

## Validation checklist

- Same INPUTFILE path works in abstract and concrete mode.
- Same topology.initialPeerType works in abstract and concrete mode.
- Concrete neighbors come from the shared topology plan, not from full-mesh discovery.
- initParameters sees a full peer vector in concrete mode.
- No code under quantas/*Peer was modified for the unification work.
- No code under quantas/Common/Abstract was modified for the unification work.
- Concrete mode produces both compact and detailed aggregate files.
- Concrete build now invalidates shared runtime objects when INPUTFILE changes.