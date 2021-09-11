#!/bin/bash
# wraps up all JS files (including json) in "JS" directory within the current directory
# into a single file following the format outlined for programs in https://github.com/CamHenlin/coprocessor.js
# requires truncate
# requires xxd

truncate --size=0 output_js
truncate --size=0 output_js.h

cd JS

for filename in *.js*; do
	echo "$filename@@@" >> ../output_js
	cat $filename >> ../output_js
	echo "&&&" >> ../output_js
done

cd ..
truncate -s-4 output_js # remove trailing &&&
xxd -C -i output_js >> output_js.h
#rm output_js
