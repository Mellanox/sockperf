#!/bin/bash -xeEl

source $(dirname $0)/globals.sh

if [ -z "$WORKSPACE" ]; then
    echo "ERROR: WORKSPACE variable is empty"
    exit 1
fi

if [ ! -d "$WORKSPACE" ]; then
    echo "ERROR: $WORKSPACE does not exist"
    exit 1
fi

HEADER_CHECK_TOOL=/opt/nvidia/header_check.py
if [ ! -f "${HEADER_CHECK_TOOL}" ]; then
    echo "ERROR: ${HEADER_CHECK_TOOL} doesn't exist!"
    exit 1
fi

cpp_files='        "extensions": [".c", ".cc", ".cpp", "c++", ".h", ".hpp", ".cs", ".inl", ".l", ".y"],'
sed -i "s/.*\"extensions\": \[\"\.c\".*/$cpp_files/g" /opt/nvidia/ProjectConfig/header-types.json

cat /opt/nvidia/ProjectConfig/header-types.json

${HEADER_CHECK_TOOL} \
  --config ${WORKSPACE}/contrib/jenkins_tests/copyright-check-map.yaml \
  --path ${WORKSPACE} \
  --git-repo ${WORKSPACE} | tee copyrights.log
exit_code=$?
echo "exit_code=${exit_code}"
# Correct error code is not returned by the script
set +eE
grep -rn ERROR copyrights.log
exit_code=$?
set -eE
if [ ${exit_code} -eq 0 ]; then
    echo "Please refer to https://confluence.nvidia.com/pages/viewpage.action?pageId=788418816"
    ${HEADER_CHECK_TOOL} \
      --config contrib/jenkins_tests/copyright-check-map.yaml \
      --path ${WORKSPACE} \
      --repair \
      --git-repo ${WORKSPACE} | tee copyrights_repair.log
    # create list of modified files
    files=$(git status | grep 'modified:' | awk '{print $NF}'  )
    mkdir $WORKSPACE/repaired_files/
    cp --parents $files $WORKSPACE/repaired_files/
    cd $WORKSPACE/repaired_files/
    tar -czf $WORKSPACE/copyright_repaired_files.tar.gz .
    exit 1
fi
