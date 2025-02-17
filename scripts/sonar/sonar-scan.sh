#!/bin/bash
set -euo pipefail

source $HOOT_HOME/scripts/sonar/sonar-config.sh

# Defaults.
USAGE=no
DEBUG_MODE=""
SONAR_BRANCH=""
SONAR_EXCLUSIONS="**/*.pb.cc,**/*.pb.h,**/*.sql"
SONAR_GITHUB_OAUTH=""
SONAR_GITHUB_PULL=""
SONAR_GITHUB_REPO="ngageoint/hootenanny"
SONAR_HOST_URL="https://sonarcloud.io"
SONAR_LOGIN=""
SONAR_ORG="hootenanny"
SONAR_PROJECT="hoot"
SONAR_SOURCES="./hoot-core,./hoot-js,./tbs,./tgs"
SONAR_THREADS=""

# Getting parameters from the command line.
while getopts ":a:b:dj:l:o:p:r:s:t:" opt; do
    case "$opt" in
        # Required parameters.
        a)
            SONAR_GITHUB_OAUTH="$OPTARG"
            ;;
        b)
            SONAR_BRANCH="$OPTARG"
            ;;
        d)
            DEBUG_MODE="--debug"
            ;;
        j)
            SONAR_PROJECT="$OPTARG"
            ;;
        l)
            SONAR_LOGIN="$OPTARG"
            ;;
        o)
            SONAR_ORG="$OPTARG"
            ;;
        p)
            SONAR_GITHUB_PULL="$OPTARG"
            ;;
        r)
            SONAR_GITHUB_REPO="$OPTARG"
            ;;
        s)
            SONAR_SOURCES="$OPTARG"
            ;;
        t)
            SONAR_THREADS="$OPTARG"
            ;;
        *)
            USAGE=yes
            ;;
    esac
done
shift $((OPTIND-1))

# Setting up.
function usage() {
    echo "sonar-scan.sh:"
    echo "  -l <Sonar Login token>"
    echo " [-a <Sonar GitHub OAuth>]"
    echo " [-b <Sonar Branch Name>]"
    echo " [-p <Sonar GitHub PR>]"
    echo " [-t <Sonar Threads>]"
    echo " [-d]  # Debug mode"
    exit 1
}

# Abort if invalid options.
if [[ "$USAGE" = "yes" || -z "$SONAR_ORG" || -z "$SONAR_GITHUB_REPO" || -z "$SONAR_PROJECT" ]]; then
    usage
fi

# Build out the scan command options.
OPTIONS=(
    "-Dsonar.projectKey=$SONAR_PROJECT"
    "-Dsonar.cfamily.build-wrapper-output=bw-output"
)

# Add gcov.reportsPath if the directory exists
if [ -d $HOOT_HOME/gcov ]; then
    OPTIONS=(
        "${OPTIONS[@]}"
        "-Dsonar.cfamily.gcov.reportsPath=$HOOT_HOME/gcov"
    )
fi

# Optional thread count parameter
if [ -n "$SONAR_THREADS" ]; then
    # NOTE: Sonar docs say that using more that `nproc` threads can actually
    # cause slowdowns, as of 4/2022 using 2 x nproc was slowing hootenanny
    # down about 2 minutes (from 31 minutes to 29 minutes) even though
    # the sonar scan process wasn't taking even 50% of each available process
    # See "Parallel Code Scan" at https://docs.sonarcloud.io/advanced-setup/languages/c-c-objective-c/
    OPTIONS=(
        "${OPTIONS[@]}"
        "-Dsonar.cfamily.threads=$SONAR_THREADS"
    )
fi

# Optional scan parameters based off parameters passed into script
if [ -n "$SONAR_LOGIN" ]; then
    # This is the hoot sonarcloud project key, requried to pass scan results to
    # server for analysis and display
    OPTIONS=(
        "${OPTIONS[@]}"
        "-Dsonar.token=$SONAR_LOGIN"
    )
fi

if [ -n "$SONAR_BRANCH" ]; then
    OPTIONS=(
        "${OPTIONS[@]}"
        "-Dsonar.branch.name=$SONAR_BRANCH"
    )
fi

if [ -n "$SONAR_GITHUB_PULL" ]; then
    # Optional pull request number that will match scan with git hub pull-request
    OPTIONS=(
        "${OPTIONS[@]}"
        "-Dsonar.github.pullRequest=$SONAR_GITHUB_PULL"
    )
fi

if [ -n "$SONAR_GITHUB_OAUTH" ]; then
    # Optional token to allow scan to post comments to github ticket
    OPTIONS=(
        "${OPTIONS[@]}"
        "-Dsonar.github.oauth=$SONAR_GITHUB_OAUTH"
    )
fi

# clean up the old folders
if [ -d $HOOT_HOME/.scannerwork ]; then
    rm -rf $HOOT_HOME/.scannerwork
fi

# run the actual scanner
JAVA_HOME=$SONAR_JDK_PATH sonar-scanner $DEBUG_MODE "${OPTIONS[@]}"
