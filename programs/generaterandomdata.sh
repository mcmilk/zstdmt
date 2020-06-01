#!/bin/sh

################################################################
# first arg random file name 
# second arg random file size
################################################################

rm -rf $1
touch $1
filesize=`ls -l $1 | awk '{ print $5 }'`
i=1
for ((;i > 0;))
do   
    filesize=`ls -l $1 | awk '{ print $5 }'`
    if [ $filesize -gt $2 ]
    then
        i=0
        echo "random filesize = $filesize"
    else 
        i=1
    fi

    head -30 /dev/urandom >> $1  
done  
