#!/bin/bash
 convert_file()
 {
    for file in `find .`
    do
        if [[ -f $file ]]
        then
            if [[ ${file##*.} == h || ${file##*.} == c || ${file##*.} == cpp ]]; 
			then
                #cp $file $file".b"
                #chmod 777 $file $file".b"
                iconv -f GB2312 -t UTF-8 $file > $file.b
                sleep 0.05
                mv $file.b $file
                chmod 777 $file
                #rm $file.b
                echo $file
            fi
        fi
    done
 }
 convert_file