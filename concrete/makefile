
EXE := test.exe
LOGGER_EXE := concrete-logger.exe
BUILD_CONTEXT_STAMP := .build-context.stamp

CXX := g++
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CXX := clang++
endif
CXXFLAGS := -pthread -std=c++17

INPUTFILE := ../quantas/AltBitPeer/AltBitUtility.json
BOOTSTRAPFILE ?= config0.json
EXPERIMENT_INDEX ?= 0

BUILD_CONTEXT = CXX=$(CXX)|CXXFLAGS=$(CXXFLAGS)|INPUTFILE=$(abspath $(INPUTFILE))|ALGS=$(strip $(ALGS))

ALGS := $(shell sed -n '/"algorithms"/,/]/p' $(INPUTFILE) \
	| sed -n 's/.*"\([^"]*\.cpp\)".*/..\/quantas\/\1/p')
ALG_OBJS := $(ALGS:.cpp=.o)

ifneq ($(filter ../quantas/AltBitPeer/AltBitPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_ALTBIT_PEER
endif
ifneq ($(filter ../quantas/BitcoinPeer/BitcoinPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_BITCOIN_PEER
endif
ifneq ($(filter ../quantas/EthereumPeer/EthereumPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_ETHEREUM_PEER
endif
ifneq ($(filter ../quantas/ExamplePeer/ExamplePeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_EXAMPLE_PEER
endif
ifneq ($(filter ../quantas/ExamplePeer/ExamplePeer2.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_EXAMPLE_PEER2
endif
ifneq ($(filter ../quantas/KademliaPeer/KademliaPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_KADEMLIA_PEER
endif
ifneq ($(filter ../quantas/LinearChordPeer/LinearChordPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_LINEARCHORD_PEER
endif
ifneq ($(filter ../quantas/PBFTPeer/PBFTPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_PBFT_PEER
endif
ifneq ($(filter ../quantas/RaftPeer/RaftPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_RAFT_PEER
endif
ifneq ($(filter ../quantas/StableDataLinkPeer/StableDataLinkPeer.cpp,$(ALGS)),)
CXXFLAGS += -DQUANTAS_CONCRETE_HAS_STABLEDATALINK_PEER
endif

OBJS := ../quantas/Common/Concrete/concreteSimulation.o ../quantas/Common/Concrete/ipUtil.o ../quantas/Common/Abstract/Channel.o $(ALG_OBJS)
LOGGER_OBJS := ../quantas/Common/Concrete/concreteLogger.o ../quantas/Common/Concrete/ipUtil.o

$(BUILD_CONTEXT_STAMP): FORCE
	@tmp=$$(mktemp); \
	printf '%s\n' '$(BUILD_CONTEXT)' > "$$tmp"; \
	if [ ! -f "$@" ] || ! cmp -s "$$tmp" "$@"; then \
		mv "$$tmp" "$@"; \
	else \
		rm -f "$$tmp"; \
	fi

$(EXE): $(OBJS)
	@$(CXX) $(CXXFLAGS) $^ -o $(EXE)

$(LOGGER_EXE): $(LOGGER_OBJS)
	@$(CXX) $(CXXFLAGS) $^ -o $(LOGGER_EXE)

%.o: %.cpp $(BUILD_CONTEXT_STAMP)
	@echo compiling $<
	@$(CXX) $(CXXFLAGS) -c $< -o $@

getIP:
	@hostname -I

release-altbit:
	@$(MAKE) release INPUTFILE=../quantas/AltBitPeer/AltBitUtility.json

release-bitcoin:
	@$(MAKE) release INPUTFILE=../quantas/BitcoinPeer/BitcoinPeerInput.json

release-ethereum:
	@$(MAKE) release INPUTFILE=../quantas/EthereumPeer/EthereumPeerInput.json

release-example:
	@$(MAKE) release INPUTFILE=../quantas/ExamplePeer/ExampleInput.json

release-kademlia:
	@$(MAKE) release INPUTFILE=../quantas/KademliaPeer/KademliaPeerInput.json

release-linearchord:
	@$(MAKE) release INPUTFILE=../quantas/LinearChordPeer/LinearChordInput.json

release-pbft:
	@$(MAKE) release INPUTFILE=../quantas/PBFTPeer/PBFTInput.json

release-raft:
	@$(MAKE) release INPUTFILE=../quantas/RaftPeer/RaftInput.json

release-stabledatalink:
	@$(MAKE) release INPUTFILE=../quantas/StableDataLinkPeer/StableDataLinkInput.json

run-altbit:
	@$(MAKE) run INPUTFILE=../quantas/AltBitPeer/AltBitUtility.json BOOTSTRAPFILE=config-local.json

run-bitcoin:
	@$(MAKE) run INPUTFILE=../quantas/BitcoinPeer/BitcoinPeerInput.json BOOTSTRAPFILE=config-bitcoin-local.json

run-ethereum:
	@$(MAKE) run INPUTFILE=../quantas/EthereumPeer/EthereumPeerInput.json BOOTSTRAPFILE=config-ethereum-local.json

run-example:
	@$(MAKE) run INPUTFILE=../quantas/ExamplePeer/ExampleInput.json BOOTSTRAPFILE=config-example-local.json

run-kademlia:
	@$(MAKE) run INPUTFILE=../quantas/KademliaPeer/KademliaPeerInput.json BOOTSTRAPFILE=config-kademlia-local.json

run-linearchord:
	@$(MAKE) run INPUTFILE=../quantas/LinearChordPeer/LinearChordInput.json BOOTSTRAPFILE=config-linearchord-local.json

run-pbft:
	@$(MAKE) run INPUTFILE=../quantas/PBFTPeer/PBFTInput.json BOOTSTRAPFILE=config-pbft-local.json

run-raft:
	@$(MAKE) run INPUTFILE=../quantas/RaftPeer/RaftInput.json BOOTSTRAPFILE=config-raft-local.json

run-stabledatalink:
	@$(MAKE) run INPUTFILE=../quantas/StableDataLinkPeer/StableDataLinkInput.json BOOTSTRAPFILE=config-stabledatalink-local.json

release: CXXFLAGS += -O3 -s
release: $(EXE) $(LOGGER_EXE)

run: kill release
	@set -e; \
	PEER_COUNT=$$(python3 -c 'import json; data=json.load(open("$(INPUTFILE)")); idx=int("$(EXPERIMENT_INDEX)"); print(data["experiments"][idx]["topology"]["initialPeers"])'); \
	./$(LOGGER_EXE) $(INPUTFILE) $(BOOTSTRAPFILE) $(EXPERIMENT_INDEX) & \
	LOGGER_PID=$$!; \
	sleep 1; \
	PEER_PIDS=""; \
	for _ in $$(seq 1 $$PEER_COUNT); do \
		./$(EXE) $(INPUTFILE) $(BOOTSTRAPFILE) -1 $(EXPERIMENT_INDEX) & \
		PEER_PIDS="$$PEER_PIDS $$!"; \
	done; \
	STATUS=0; \
	for PID in $$PEER_PIDS; do \
		wait $$PID || STATUS=$$?; \
	done; \
	wait $$LOGGER_PID || STATUS=$$?; \
	exit $$STATUS

kill:
	@USER=$$(whoami); \
	PIDS=$$(pgrep -u "$$USER" -f '(^|/)(test|concrete-logger)\.exe([[:space:]]|$$)' || true); \
	COUNT=$$(printf '%s\n' "$$PIDS" | awk 'NF {count++} END {print count + 0}'); \
	if [ -n "$$PIDS" ]; then \
		echo "Killing $$COUNT concrete process(es) started by $$USER..."; \
		kill $$PIDS; \
	else \
		echo "No running concrete processes found for user $$USER."; \
	fi

status:
	@USER=$$(whoami); \
	PIDS=$$(pgrep -u "$$USER" -f '(^|/)(test|concrete-logger)\.exe([[:space:]]|$$)' || true); \
	COUNT=$$(printf '%s\n' "$$PIDS" | awk 'NF {count++} END {print count + 0}'); \
	if [ -n "$$PIDS" ]; then \
		echo "$$COUNT concrete process(es) running for $$USER..."; \
	else \
		echo "No running concrete processes found for user $$USER."; \
	fi

clean:
	@$(RM) $(EXE) $(LOGGER_EXE) $(BUILD_CONTEXT_STAMP) $(OBJS) $(LOGGER_OBJS)

compare-aggregates:
	@if [ -z "$(ABSTRACT)" ] || [ -z "$(CONCRETE)" ]; then \
		echo "Usage: make compare-aggregates ABSTRACT=/path/to/abstract.json CONCRETE=/path/to/concrete.json [REPORT=/path/to/report.json]"; \
		exit 1; \
	fi
	@python3 aggregate_tools.py compare "$(ABSTRACT)" "$(CONCRETE)" $(if $(REPORT),--report "$(REPORT)",)

prepare-abstract-config:
	@if [ -z "$(OUTPUTFILE)" ] || [ -z "$(LOGFILE)" ]; then \
		echo "Usage: make prepare-abstract-config INPUTFILE=... OUTPUTFILE=/tmp/app-abstract.json LOGFILE=/abs/or/relative/output.json [EXPERIMENT_INDEX=0]"; \
		exit 1; \
	fi
	@python3 aggregate_tools.py prepare-abstract "$(INPUTFILE)" "$(OUTPUTFILE)" "$(LOGFILE)" --experiment-index "$(if $(EXPERIMENT_INDEX),$(EXPERIMENT_INDEX),0)"

.PHONY: FORCE getIP release run kill status clean compare-aggregates prepare-abstract-config \
	release-altbit release-bitcoin release-ethereum release-example release-kademlia \
	release-linearchord release-pbft release-raft release-stabledatalink \
	run-altbit run-bitcoin run-ethereum run-example run-kademlia run-linearchord \
	run-pbft run-raft run-stabledatalink
