# TODO

add more to the tests to verify the results instead of just making sure the program runs

add ability to "skip" ahead a large number of rounds delivering all the necessary messages. This would help to simulate more realistic conditions in some algorithms. Such as in bitcoin where it takes 10 minutes to mine a block but only 1s to send a round trip message. So 3600 rounds need to pass between blocks being mined when compared to sending messages.

move to init parameters and end of round running for each peer individually and then compute the results after for graphing instead of while running to make more compatible with distrubted simulation

Allow the input files to be used the same way for concrete and abstract simulation to allow an algorithm to do both

Concrete-mode refactor direction:

- Parse the same experiment JSON in concrete mode and treat it as the only source of truth for topology, parameters, peer type, rounds, and algorithms.
- Keep bootstrap data concrete-specific and limited to deployment details such as leader address and optional local port.
- Build the concrete neighbor set from the experiment topology instead of forming a full mesh on discovery.
- Instantiate peers by the same name as abstract mode and inject NetworkInterfaceConcrete in the concrete runtime.
- Create shadow peers with a no-op interface so initParameters still receives a full peer vector.
- Treat exact end-of-round aggregation as a later step unless a generic metrics snapshot API is introduced.