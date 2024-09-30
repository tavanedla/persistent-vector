PersistentVector is an implementation of a vector class that persists the vector to disk, so that it is resilient to crashes. It works by writing every mutation to a log file, in addition to mutating the vector in-memory.

Potential future work -
1. Optimize the vector for erases. Currently erase is an O(n) operation.
2. Make it thread-safe
3. Periodically snapshot log file.
