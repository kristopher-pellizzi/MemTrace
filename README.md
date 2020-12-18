# MemTraceThesis

## Execution tests with coreutils
In order to run test.sh, you'll need to compile coreutils utilities inside Tests folder. To do that, follow these steps:
- cd into Tests/
- Run ./boostrap
- If it returns errors telling that programs or utilities are missing, install those (e.g. through apt-get)
- Once you have all prerequisites, and you have correctly run ./bootstrap, run ./configure
- Run make

At this point, everything should be ready, you can cd .. and run ./test.sh

***NOTE:** it may take a while*
