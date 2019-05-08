//
//  PBFTPeer_Sharded_Test.hpp
//  BlockGuard
//
//  Created by Kendric Hood on 5/7/19.
//  Copyright © 2019 Kent State University. All rights reserved.
//

#ifndef PBFTPeer_Sharded_Test_hpp
#define PBFTPeer_Sharded_Test_hpp

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include "../BlockGuard/PBFTPeer_Sharded.hpp"

void RunPBFTPeerShardedTest      (std::string filepath); // run all PBFT tests

// test for committee PBFT running
void oneRequestOneCommittee     (std::ostream &log);
void oneRequestMultiCommittee   (std::ostream &log);
void MultiRequestMultiCommittee (std::ostream &log);

// These are copies of the test from PBFT Peer as of 5/7
void constructors               (std::ostream &log);// test basic peer constructors
void testSettersMutators        (std::ostream &log);// test setters and test Mutators

// test for group and committee assignment
void testGroups                 (std::ostream &log);// test that peers can be added and removed from a group
void testCommittee              (std::ostream &log);// test that peers can be added and removed from a committee

#endif /* PBFTPeer_Sharded_Test_hpp */