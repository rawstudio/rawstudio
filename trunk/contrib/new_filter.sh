#!/bin/bash 
# bash is needed for timeout in 'read'

MAKEFILETEMPLATE=filter-Makefile-template.am
FILTERTEMPLATE=filter-template.c

FILTERPATH=../plugins

NAME=${1}
Name=${2}
name=${3}


if [ -z ${3} ]; then
    echo ${0} NAME Name name # FIXME: This should be done with only one name
    exit 1
fi

if [ ! -e ${MAKEFILETEMPLATE} -o ! -e ${FILTERTEMPLATE} ]; then
    echo "This script needs to be run from the contrib/ directory..."
    exit 2
fi

if [ -d ${FILTERPATH}/${name} ]; then
    echo -n "Target ${FILTERPATH}/${name} exists, remove? [N/y] "
    read -t 10 TEMP
    TEMP=$(echo ${TEMP}|tr [:upper:] [:lower:])
    if [ "${TEMP}" = "y" ]; then
	rm -rf ${FILTERPATH}/${name}
    else
	exit 3
    fi
fi

mkdir ${FILTERPATH}/${name}
sed "s/TEMPLATE/${NAME}/g;s/Template/${Name}/g;s/template/${name}/g" ${FILTERTEMPLATE} > ${FILTERPATH}/${name}/${name}.c
sed "s/TEMPLATE/${NAME}/g;s/Template/${Name}/g;s/template/${name}/g" ${MAKEFILETEMPLATE} > ${FILTERPATH}/${name}/Makefile.am

echo "${FILTERPATH}/${name}/${name}.c has been created, please edit configure.in and plugins/Makefile.am to enable filter"
