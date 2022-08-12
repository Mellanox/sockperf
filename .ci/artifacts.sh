#!/bin/bash -xl

if [ -d jenkins ]; then
    gzip -f ./jenkins/*.tar 2>/dev/null || true
    cd ./jenkins/ ;
        for f in *.tar.gz ; do [ -e "$f" ] && mv "$f" "${flags}/arch-${name}-$f" ; done ;
    cd ..
    cd ./jenkins/${flags};
        for f in *.tap ; do [ -e "$f" ] && mv "$f" "${flags}-${name}-$f" ; done ;
        for f in *.xml ; do [ -e "$f" ] && mv "$f" "${flags}-${name}-$f" ; done ;
    cd ../..
fi
