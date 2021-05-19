Usage:

To test a 6CCL machine, use this commandline:
    IS_8CCL="N" ./test.sh 2>&1 | tee log.txt

To test a 8CCL machine, use this commandline:
    IS_8CCL="Y" ./test.sh 2>&1 | tee log.txt


Other commandline configurable parameters include:
1. STREAM_CMD
2. LAT_PIPE_CMD
3. LOCK_CMD

Please read 'test.sh' to get details.
