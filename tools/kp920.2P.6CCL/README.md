These benchmarks in this folder is captured on the following machine:
 - Kunpeng 920:
	 - 96 cores, in 4 Dies (NUMA nodes).
	 - 6 CCLs (clusters) in each NUMA node.
	 - 4 cores in each CCL.
 - Memory: 512GB.
	 - All DIMM 0 sockets are mounted with a 32GB memory board.
	 - All DIMM 1 sockets are empty.
	 - Detials:
```
SOCKET 0 CHANNEL 0 DIMM 0
Samsung, 32GB, 2933MHz, 1.2V/2.0V   (M393A4K40CB2-CVF)
SOCKET 0 CHANNEL 0 DIMM 1
None
SOCKET 0 CHANNEL 1 DIMM 0
Samsung, 32GB, 2933MHz, 1.2V/2.0V   (M393A4K40CB2-CVF)
SOCKET 0 CHANNEL 1 DIMM 1
None
SOCKET 0 CHANNEL 2 DIMM 0
Hynix, 32GB, 2933MHz, 1.2V/2.0V  	(HMA84GR7JJR4N-WM)
SOCKET 0 CHANNEL 2 DIMM 1
None
SOCKET 0 CHANNEL 3 DIMM 0
Hynix, 32GB, 2933MHz, 1.2V/2.0V  	(HMA84GR7JJR4N-WM)
...
... (Note All the remaining DIMM 1’s are Hynix)
... (Note All the remaining DIMM 0’s are empty.)
SOCKET 1 CHANNEL 7 DIMM 1
None
```

== END ==

