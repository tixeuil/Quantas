
EXE := test.exe
LOGGER_EXE := concrete-logger.exe

CXX := g++
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CXX := clang++
endif
CXXFLAGS := -pthread -std=c++17

INPUTFILE := ../quantas/AltBitPeer/AltBitUtility.json
BOOTSTRAPFILE := config0.json

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

$(EXE): $(OBJS)
	@$(CXX) $(CXXFLAGS) $^ -o $(EXE)

$(LOGGER_EXE): $(LOGGER_OBJS)
	@$(CXX) $(CXXFLAGS) $^ -o $(LOGGER_EXE)

%.o: %.cpp
	@echo compiling $<
	@$(CXX) $(CXXFLAGS) -c $< -o $@

getIP:
	@hostname -I

release: CXXFLAGS += -O3 -s
release: $(EXE) $(LOGGER_EXE)

run: kill release
	@./$(LOGGER_EXE) $(INPUTFILE) $(BOOTSTRAPFILE) &
	@./$(EXE) $(INPUTFILE) $(BOOTSTRAPFILE) 5000 &
	@./$(EXE) $(INPUTFILE) $(BOOTSTRAPFILE) &

kill:
	@USER=$$(whoami); \
	PIDS=$$(ps -eo user,pid,command | awk -v user="$$USER" '$$1 == user && $$3 ~ /^\.\/$(EXE)$$/ {print $$2}'); \
	COUNT=$$(echo "$$PIDS" | wc -w); \
	if [ -n "$$PIDS" ]; then \
		echo "Killing $$COUNT process(es) of $(EXE) started by $$USER..."; \
		kill $$PIDS; \
	else \
		echo "No running $(EXE) processes found for user $$USER."; \
	fi

status:
	@USER=$$(whoami); \
	PIDS=$$(ps -eo user,pid,command | awk -v user="$$USER" '$$1 == user && $$3 ~ /^\.\/$(EXE)$$/ {print $$2}'); \
	COUNT=$$(echo "$$PIDS" | wc -w); \
	if [ -n "$$PIDS" ]; then \
		echo "$$COUNT process(es) of $(EXE) started by $$USER..."; \
	else \
		echo "No running $(EXE) processes found for user $$USER."; \
	fi
