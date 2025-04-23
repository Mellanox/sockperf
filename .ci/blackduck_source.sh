#!/bin/bash -Exel

# Check if the variables and pipeline attributes are set
[[ -z "${WORKSPACE}" ]] && { echo "Error: WORKSPACE variable is not set"; exit 1; }
[[ -z "$BLACKDUCK_API_TOKEN" ]] && { echo "Error: BLACKDUCK_API_TOKEN variable is not set"; exit 1; }
[[ ! -d "${WORKSPACE}/logs" ]] && mkdir -p "${WORKSPACE}/logs"

# Create valid JSON for further authentication in BlackDuck server
json=$(jq -n \
  --arg token "${BLACKDUCK_API_TOKEN}" \
  '{"blackduck.url": "https://blackduck.mellanox.com/", "blackduck.api.token": $token }')

export SPRING_APPLICATION_JSON="${json}"
export PROJECT_NAME=LibVMA
export PROJECT_VERSION="${sha1}"
export PROJECT_SRC_PATH="${WORKSPACE}"/src/

echo "Running BlackDuck (SRC) on ${name}"
echo "CONFIG:"
echo "        NAME: ${PROJECT_NAME}"
echo "     VERSION: ${PROJECT_VERSION}"
echo "    SRC_PATH: ${PROJECT_SRC_PATH}"

# clone BlackDuck
[[ -d /tmp/blackduck ]] && rm -rf /tmp/blackduck
chmod 600 "${GERRIT_SSH_KEY}"
SSH_CMD="ssh -i ${GERRIT_SSH_KEY} -l swx-jenkins2-svc -o StrictHostKeyChecking=no"
git clone -c core.sshCommand="${SSH_CMD}" -b master --single-branch --depth=1 ssh://git-nbu.nvidia.com:12023/DevOps/Tools/blackduck /tmp/blackduck
cd /tmp/blackduck

# disable check errors
set +e
timeout 3600 ./run_bd_scan.sh
exit_code=$?
# enable back
set -e

# copy run log to a place that jenkins job will archive it
REPORT_NAME="BlackDuck_source_${PROJECT_NAME}_${PROJECT_VERSION}"
cat "log/${PROJECT_NAME}_${PROJECT_VERSION}"*.log > "${WORKSPACE}/logs/${REPORT_NAME}.log" || true
cat "log/${PROJECT_NAME}_${PROJECT_VERSION}"*.log || true

if [ "${exit_code}" == "0" ]; then
    cp -v /tmp/blackduck/report/*.pdf "${WORKSPACE}/logs/${REPORT_NAME}.pdf"
fi

exit "${exit_code}"
