#!/bin/bash

# source parameters
MISSIONS_HOME="../.."
source ${MISSIONS_HOME}/trunk/config/hard_config
source ${MISSIONS_HOME}/trunk/config/soft_config

HELP="no"
JUST_BUILD="no"
BAD_ARGS=""
WARP=1

#-------------------------------------------------------
#  Part 1: Check for and handle command-line arguments
#-------------------------------------------------------
let COUNT=0
for ARGI; do
    UNDEFINED_ARG=$ARGI
    if [ "${ARGI}" = "--help" -o "${ARGI}" = "-h" ] ; then
	HELP="yes"
	UNDEFINED_ARG=""
    fi
    if [ "${ARGI}" = "--just_build" -o "${ARGI}" = "-j" ] ; then
	JUST_BUILD="yes"
	UNDEFINED_ARG=""
    fi
    if [ "${UNDEFINED_ARG}" != "" ] ; then
	BAD_ARGS=$UNDEFINED_ARG
    fi
done

if [ "${BAD_ARGS}" != "" ] ; then
    printf "Bad Argument: %s \n" $BAD_ARGS
    exit 0
fi

if [ "${HELP}" = "yes" ]; then
    printf "%s [SWITCHES]            \n" $0
    printf "Switches:                \n"
    printf "  --just_build, -j       \n" 
    printf "  --help, -h             \n" 
    exit 0;
fi

#-------------------------------------------------------
#  Part 2: Create the shoreside.moos file
#-------------------------------------------------------

nsplug meta_shoreside.moos targ_shoreside.moos -f       \
    LPORT=$SLPORT      VPORT=$SPORT                     \
    VNAME=$SNAME       WARP=$WARP                       \
    VHOST=$SHOREHOST                                    \
    VNAME2=${HARD_CONFIG["NOSTROMO:VNAME"]}             \
    VHOST2=${HARD_CONFIG["NOSTROMO:VHOST"]}             \
    LPORT2=${HARD_CONFIG["NOSTROMO:LPORT"]}             \
    VNAME3=${HARD_CONFIG["SILVANA:VNAME"]}              \
    VHOST3=${HARD_CONFIG["SILVANA:VHOST"]}              \
    LPORT3=${HARD_CONFIG["SILVANA:LPORT"]}              \
    VNAME4=${HARD_CONFIG["KESTREL:VNAME"]}              \
    VHOST4=${HARD_CONFIG["KESTREL:VHOST"]}              \
    LPORT4=${HARD_CONFIG["KESTREL:LPORT"]}              \

if [ ${JUST_BUILD} = "yes" ] ; then
    exit 0
fi

#-------------------------------------------------------
#  Part 3: Launch the processes
#-------------------------------------------------------

printf "Launching $SNAME MOOS Community \n"
pAntler targ_shoreside.moos >& /dev/null &

#-------------------------------------------------------
#  Part 4: Exiting and/or killing the simulation
#-------------------------------------------------------

ANSWER="0"
while [ "${ANSWER}" != "1" -a "${ANSWER}" != "2" ]; do
    printf "Now what? (1) Exit script (2) Exit and Kill-All \n"
    printf "> "
    read ANSWER
done

# %1, matches the PID of the job in the active jobs list, 
# namely the pAntler job launched in Part 3.

if [ "${ANSWER}" = "2" ]; then
    printf "Killing all processes ... \n "
    kill %1 
    printf "Done killing processes.   \n "
fi
