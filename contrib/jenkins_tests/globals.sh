#!/bin/bash

WORKSPACE=${WORKSPACE:=$PWD}
if [ -z "$BUILD_NUMBER" ]; then
    echo Running interactive
    BUILD_NUMBER=1
    WS_URL=file://$WORKSPACE
    JENKINS_RUN_TESTS=yes
else
    echo Running under jenkins
    WS_URL=$JOB_URL/ws
fi

TARGET=${TARGET:=all}
i=0
if [ "$TARGET" == "all" -o "$TARGET" == "default" ]; then
	target_list[$i]="default: "
	i=$((i+1))
fi


# exit code
rc=0

jenkins_test_custom_configure=${jenkins_test_custom_configure:=""}
jenkins_test_custom_prefix=${jenkins_test_custom_prefix:="jenkins"}

prefix=${jenkins_test_custom_prefix}/${jenkins_target}
build_dir=${WORKSPACE}/${prefix}/build/
install_dir=${WORKSPACE}/${prefix}/install
compiler_dir=${WORKSPACE}/${prefix}/compiler
test_dir=${WORKSPACE}/${prefix}/test
rpm_dir=${WORKSPACE}/${prefix}/rpm
cov_dir=${WORKSPACE}/${prefix}/cov
cppcheck_dir=${WORKSPACE}/${prefix}/cppcheck
csbuild_dir=${WORKSPACE}/${prefix}/csbuild
style_dir=${WORKSPACE}/${prefix}/style


nproc=$(grep processor /proc/cpuinfo|wc -l)
make_opt="-j$(($nproc / 2 + 1))"
if [ $(command -v timeout >/dev/null 2>&1 && echo $?) ]; then
    timeout_exe="timeout -s SIGKILL 20m"
fi

trap "on_exit" INT TERM ILL KILL FPE SEGV ALRM

function on_exit()
{
    rc=$((rc + $?))
    echo "[${0##*/}]..................exit code = $rc"
    pkill -9 sockperf
}

function do_cmd()
{
    cmd="$*"
    set +e
    eval $cmd >> /dev/null 2>&1
    ret=$?
    set -e
    if [ $ret -gt 0 ]; then
        exit $ret
    fi
}

function do_export()
{
    export PATH="$1/bin:${PATH}"
    export LD_LIBRARY_PATH="$1/lib:${LD_LIBRARY_PATH}"
    export MANPATH="$1/share/man:${MANPATH}"
}

function do_archive()
{
    cmd="tar -rvf ${jenkins_test_artifacts}.tar $*"
    set +e
    eval $cmd >> /dev/null 2>&1
    set -e
}

function do_github_status()
{
    echo "Calling: github $1"
    eval "local $1"

    local token=""
    if [ -z "$tokenfile" ]; then
        tokenfile="$HOME/.mellanox-github"
    fi

    if [ -r "$tokenfile" ]; then
        token="$(cat $tokenfile)"
    else
        echo Error: Unable to read tokenfile: $tokenfile
        return
    fi

    curl \
    -X POST \
    -H "Content-Type: application/json" \
    -d "{\"state\": \"$state\", \"context\": \"$context\",\"description\": \"$info\", \"target_url\": \"$target_url\"}" \
    "https://api.github.com/repos/$repo/statuses/${sha1}?access_token=$token"
}

# Test if an environment module exists and load it if yes.
# Otherwise, return error code.
# $1 - module name
#
function do_module()
{
    echo "Checking module $1"
    if [ $(command -v module >/dev/null 2>&1 || echo $?) ]; then
	    echo "[SKIP] module tool does not exist"
	    exit 0
	else
        module load "$1"
    fi
}

# format text
#
function do_format()
{
    set +x
    local is_format=true
    if [[ $is_format == true ]] ; then
        res=""
        for ((i=2; i<=$#; i++)) ; do
            case "${!i}" in
                "bold" ) res="$res\e[1m" ;;
                "underline" ) res="$res\e[4m" ;;
                "reverse" ) res="$res\e[7m" ;;
                "red" ) res="$res\e[91m" ;;
                "green" ) res="$res\e[92m" ;;
                "yellow" ) res="$res\e[93m" ;;
            esac
        done
        echo -e "$res$1\e[0m"
    else
        echo "$1"
    fi
    set -x
}

# print error message
#
function do_err()
{
    set +x
    echo -e $(do_format "FAILURE: $1" "red" "bold") 2>&1
    if [ -n "$2" ]; then
        echo ">>>"
        cat $2
        echo ">>>"
    fi
    set -x
}

# Verify if current environment is suitable.
#
function do_check_env()
{
    echo "Checking system configuration"
    if [ $(command -v pkill >/dev/null 2>&1 || echo $?) ]; then
        echo "pkill is not found"
        echo "environment [NOT OK]"
        exit 1
    fi
    if [ $(sudo pwd >/dev/null 2>&1 || echo $?) ]; then
        echo "sudo does not work"
        echo "environment [NOT OK]"
        exit 1
    fi

    echo "environment [OK]"
}

# Check if the unit should be proccesed
# $1 - output message
# $2 - [on|off] if on - skip this case if JENKINS_RUN_TESTS variable is OFF
#
function do_check_filter()
{
    local msg=$1
    local filter=$2

    if [ -n "$filter" -a "$filter" == "on" ]; then
        if [ -z "$JENKINS_RUN_TESTS" -o "$JENKINS_RUN_TESTS" == "no" ]; then
            echo "$msg [SKIP]"
            exit 0
        fi
    fi

    echo "$msg [OK]"
}

# Launch command and detect result of execution
# $1 - test command
# $2 - test id
# $3 - test name
# $4 - test tap file
# $5 - files for stdout/stderr
#
function do_check_result()
{
    set +e
    if [ -z "$5" ]; then
        eval $timeout_exe $1
        ret=$?
    else
        eval $timeout_exe $1 2>> "${5}.err" 1>> "${5}.log"
        ret=$?
        do_archive "${5}.err" "${5}.log"
    fi
    set -e
    if [ $ret -gt 0 ]; then
        echo "not ok $2 $3" >> $4
        if [ -z "$5" ]; then
            do_err "$1"
        else
            do_err "$1" "${5}.err"
        fi
    else
        echo "ok $2 $3" >> $4
    fi
    rc=$((rc + $ret))
}

# Detect interface ip
# $1 - [ib|eth] to select link type or empty to select the first found
# $2 - [empty|mlx4|mlx5]
# $3 - ip address not to get
#
function do_get_ip()
{
    for ip in $(ibdev2netdev | grep Up | grep "$2" | cut -f 5 -d ' '); do
        if [ -n "$1" -a "$1" == "ib" -a -n "$(ip link show $ip | grep 'link/inf')" ]; then
            found_ip=$(ip -4 address show $ip | grep 'inet' | sed 's/.*inet \([0-9\.]\+\).*/\1/')
            if [ -n "$(ibdev2netdev | grep $ip | grep mlx5)" ]; then
                local ofed_v=$(ofed_info -s | grep OFED | sed 's/.*[l|X]-\([0-9\.]\+\).*/\1/')
                if [ $(echo $ofed_v | grep 4.[1-9] >/dev/null 2>&1 || echo $?) ]; then
                    echo "$ip is CX4 device that does not support IPoIB in OFED: $ofed_v"
                    unset found_ip
                fi
            fi
        elif [ -n "$1" -a "$1" == "eth" -a -n "$(ip link show $ip | grep 'link/eth')" ]; then
            found_ip=$(ip -4 address show $ip | grep 'inet' | sed 's/.*inet \([0-9\.]\+\).*/\1/')
        elif [ -z "$1" ]; then
            found_ip=$(ip -4 address show $ip | grep 'inet' | sed 's/.*inet \([0-9\.]\+\).*/\1/')
        fi
        if [ -n "$found_ip" -a "$found_ip" != "$3" ]; then
            echo $found_ip
            break
        fi
    done
}
