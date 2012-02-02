#!/usr/bin/env bash
SIZES="128"
COUNT=10000
LAT_TESTS="pipe_lat tcp_lat unix_lat mempipe_lat tcp_nodelay_lat"
TESTS="${LAT_TESTS}"

NUM_CORES=$2

ODIR=$1
rm -rf ${ODIR}
mkdir -p ${ODIR}

for SIZE in ${SIZES}; do
  c1=0
  while [[ ${c1} -lt ${NUM_CORES} ]]; do
    c2=0
    while [[ ${c2} -lt ${NUM_CORES} ]]; do
      echo ${c1} to ${c2}
      for t in ${TESTS}; do
        d="${ODIR}/${SIZE}/${c1}-${c2}-${t}"
        mkdir -p ${d}
        if [ ${c1} -eq ${c2} ]; then
          count=100
        else
          count=${COUNT}
        fi
        ./${t} -t -s ${SIZE} -c ${count} -a ${c1} -b ${c2} -o ${d}
      done
      c2=$(expr ${c2} + 1)
    done
    c1=$(expr ${c1} + 1)
  done

  # post-processing
  for t in ${TESTS}; do
    ofile=${ODIR}/${t}.csv
    c1=0
    while [[ ${c1} -lt ${NUM_CORES} ]]; do
      c2=0
      while [[ ${c2} -lt ${NUM_CORES} ]]; do
        d="${ODIR}/${SIZE}/${c1}-${c2}-${t}/01-${t}-headline.log"
        speed=`tail -1 ${d} | awk '{print $4}' | sed -e 's/s//g'`
        echo -n "${speed} " >> ${ofile}
        c2=$(expr ${c2} + 1)
      done
      echo "" >> ${ofile}
      c1=$(expr ${c1} + 1)
    done
  done

done
