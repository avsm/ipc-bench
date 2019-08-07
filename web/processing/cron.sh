#!/bin/bash

CUR_DIR=$(pwd)

RESULTS_DIR=../../results
OUT_DIR=/usr/groups/netos/html/projects/ipc-bench
HTML_OUTFILE=results.html

cd ${RESULTS_DIR}
RESULTS=$(ls tmp*.tar.gz)
#RESULTS="tmpsbkFiM.tar.gz"
cd ${CUR_DIR}

# Generate results.html
cat ${PWD}/html_head.tmpl > ${OUT_DIR}/${HTML_OUTFILE}.new
cat ${PWD}/html_results_head.tmpl >> ${OUT_DIR}/${HTML_OUTFILE}.new
for r in ${RESULTS}; do
  ARCHIVE=${r}
  NAME=$(echo ${r} | sed 's/\.tar\.gz//g') DIR=tmp/${NAME}
  if [[ ! -f ${OUT_DIR}/details/${NAME}.html ]]; then
    echo -n "Extracting ${ARCHIVE}..."
    tar -xzf ${RESULTS_DIR}/${ARCHIVE}
    echo " done!"
    # Check the extracted data directory exists
    if [[ ! -d ${DIR} ]]; then
      echo "FATAL: After extraction, ${DIR} did not exist!"
    fi
  fi

  # Find the target cores used
  TARGET_CPUS=$(cat ${DIR}/logs/target_cpus)

  # Now generate the results overview page
  python gen-overview-entry.py ${DIR} ${OUT_DIR}/${HTML_OUTFILE}.new \
    ${TARGET_CPUS} ${NAME}

  # Generate details page
  if [[ ! -f ${OUT_DIR}/details/${NAME}.html ]]; then
    mkdir -p ${OUT_DIR}/details
    mkdir -p ${DIR}/graphs
    cat ${PWD}/html_head.tmpl > ${OUT_DIR}/details/${NAME}.html
    python gen-details-page.py ${DIR} ${OUT_DIR}/details ${TARGET_CPUS} ${NAME}
    cat ${PWD}/html_foot.tmpl >> ${OUT_DIR}/details/${NAME}.html

    # Move graphs
    mkdir -p ${OUT_DIR}/graphs
    rm -rf ${OUT_DIR}/graphs/${NAME}
    mv ${DIR}/graphs ${OUT_DIR}/graphs/${NAME}
    chmod -R g+rx ${OUT_DIR}/graphs/${NAME}
    chmod -R o+rx ${OUT_DIR}/graphs/${NAME}
  fi
done
cat ${PWD}/html_results_foot.tmpl >> ${OUT_DIR}/${HTML_OUTFILE}.new
cat ${PWD}/html_foot.tmpl >> ${OUT_DIR}/${HTML_OUTFILE}.new

mv ${OUT_DIR}/${HTML_OUTFILE}.new ${OUT_DIR}/${HTML_OUTFILE}


